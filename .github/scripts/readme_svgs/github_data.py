import os
import requests
import logging
import json
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import List, Dict, Optional, Any, Tuple

#----- Configuration -----#
REPO_OWNER = "brenocq"
REPO_NAME = "implot3d"
GITHUB_TOKEN = os.environ.get('GITHUB_TOKEN')
API_BASE_URL_REST = "https://api.github.com/"
API_BASE_URL_GRAPHQL = "https://api.github.com/graphql"
if not GITHUB_TOKEN:
    raise ValueError("GITHUB_TOKEN environment variable is not set")

HEADERS_GRAPHQL = {
    "Authorization": f"Bearer {GITHUB_TOKEN}",
    "Content-Type": "application/json"
}

HEADERS_REST = {
    "Authorization": f"Bearer {GITHUB_TOKEN}",
    "Accept": "application/vnd.github.v3+json",
    "X-GitHub-Api-Version": "2022-11-28"
}

#----- Logging -----#
logging.basicConfig(level=logging.DEBUG, format='[%(levelname)s] %(message)s')

#----- Data Classes -----#
class IssueType(Enum):
    BUG = auto()
    FEATURE = auto()
    OTHER = auto()

class IssueState(Enum):
    OPEN = auto()
    COMPLETED = auto()
    CLOSED = auto()

class PrState(Enum):
    OPEN = auto()
    CLOSED = auto()
    MERGED = auto()

@dataclass
class Label:
    name: str
    color: str # Hex color with # prefix

@dataclass
class Reactions:
    plus_one: int = 0
    minus_one: int = 0
    laugh: int = 0
    hooray: int = 0
    confused: int = 0
    heart: int = 0
    rocket: int = 0
    eyes: int = 0

@dataclass
class Issue:
    number: int
    title: str # Without the [Feature] or [Bug] prefix
    type: IssueType
    state: IssueState
    url: str
    author: str
    created_at: str # ISO 8601 format string
    labels: List[Label]
    contributors: List[str]
    contributors_avatars: List[str]
    reactions: Reactions
    is_pinned: bool = False
    completed_tasks: int = 0
    total_tasks: int = 0
    comment_count: int = 0
    last_commenter: Optional[str] = None
    last_commented_at: Optional[str] = None # ISO 8601 format string
    linked_branch: Optional[str] = None
    linked_pr: Optional[int] = None
    linked_pr_state: Optional[PrState] = None
    commit_count: Optional[int] = None
    files_changed: Optional[int] = None
    additions: Optional[int] = None
    deletions: Optional[int] = None

@dataclass
class StatusLabel:
    name: str
    color: str
    issue_count: int

#----- Branch/PR stats -----#
def get_branch_stats(branch: str) -> Optional[Tuple[int, int, int, int]]:
    """
    Fetches stats for a specific branch.

    Args:
        branch: The name of the branch.

    Returns:
        A tuple containing:
        (commit_count, files_changed, total_additions, total_deletions)
        Returns None if fetching fails.
    """

    compare_url = f"{API_BASE_URL_REST}/repos/{REPO_OWNER}/{REPO_NAME}/compare/main...{branch}"

    ahead_by = 0
    total_additions = 0
    total_deletions = 0

    try:
        response = requests.get(compare_url, headers=HEADERS_REST)
        response.raise_for_status() # Raise HTTPError for bad responses (4xx or 5xx)
        data = response.json()

        ahead_by = data.get('ahead_by', 0)

        files_changed = data.get('files', [])
        if files_changed: # Ensure the key exists and the list is not empty
            for file_info in files_changed:
                total_additions += file_info.get('additions', 0)
                total_deletions += file_info.get('deletions', 0)
        return ahead_by, len(files_changed), total_additions, total_deletions
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 404:
             logging.error(f"Comparison failed: Branch '{branch}' not found (404). Error: {e}")
        else:
             logging.error(f"Error fetching comparison from GitHub API: {e}")
    except requests.exceptions.RequestException as e:
         logging.error(f"Network or Request error fetching comparison: {e}")
    except Exception as e:
        logging.error(f"An unexpected error occurred during comparison: {e}")

    return None

def get_pr_stats(pr_number: int) -> Optional[Tuple[int, int, int, int]]:
    """
    Fetches stats for a specific Pull Request.

    Args:
        pr_number: The number of the pull request.

    Returns:
        A tuple containing:
        (commit_count, files_changed, total_additions, total_deletions)
        Returns None if fetching fails.
    """
    pr_url = f"{API_BASE_URL_REST}/repos/{REPO_OWNER}/{REPO_NAME}/pulls/{pr_number}"

    try:
        response = requests.get(pr_url, headers=HEADERS_REST)
        response.raise_for_status() # Raise HTTPError for bad responses (4xx or 5xx)
        data = response.json()

        # Extract the stats directly from the PR data
        commit_count = data.get('commits', 0)
        files_changed = data.get('changed_files', 0)
        total_additions = data.get('additions', 0)
        total_deletions = data.get('deletions', 0)

        return commit_count, files_changed, total_additions, total_deletions

    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 404:
             logging.error(f"Fetching PR failed: PR #{pr_number} not found in {REPO_OWNER}/{REPO_NAME} (404). Error: {e}")
        else:
             logging.error(f"Error fetching PR details from GitHub API ({e.response.status_code}): {e}")
    except requests.exceptions.RequestException as e:
         logging.error(f"Network or Request error fetching PR details: {e}")
    except Exception as e:
        logging.error(f"An unexpected error occurred fetching PR details: {e}")

    return None

#----- Parsing Helper -----#
def parse_issue_from_data(issue_data: Dict[str, Any]) -> Issue:
    """Parses raw API dict into an Issue dataclass, requires post-processing."""

    #--- Basic Fields ---#
    state_enum = IssueState.OPEN if issue_data.get('state') == 'OPEN' else IssueState.COMPLETED
    if issue_data.get('stateReason') == 'NOT_PLANNED':
        state_enum = IssueState.CLOSED
    comment_count = issue_data.get('comments', {}).get('totalCount', 0)
    author_login = issue_data.get('author', {}).get('login', 'unknown')
    author_avatar = issue_data.get('author', {}).get('avatarUrl', '')
    updated_at = issue_data.get('updated_at', '')

    #--- Issue title & type ---#
    title = issue_data.get('title', '')
    issue_type = IssueType.OTHER
    if title.startswith("[Feature]"):
        issue_type = IssueType.FEATURE
        title = title.replace("[Feature]", "").strip()
    elif title.startswith("[Bug]"):
        issue_type = IssueType.BUG
        title = title.replace("[Bug]", "").strip()

    #--- Labels ---#
    parsed_labels = [
        Label(name=lbl['name'], color=f"#{lbl['color']}")
        for lbl in issue_data.get('labels', {}).get('nodes', [])
    ]

    #--- Reactions ---#
    counts = {
        "THUMBS_UP": 0,
        "THUMBS_DOWN": 0,
        "LAUGH": 0,
        "HOORAY": 0,
        "CONFUSED": 0,
        "HEART": 0,
        "ROCKET": 0,
        "EYES": 0,
    }
    raw_reactions = issue_data.get('reactionGroups', [])
    for raw_rection in raw_reactions:
        counts[raw_rection['content']] = raw_rection['reactors']['totalCount']
    parsed_reactions = Reactions(
        plus_one=counts.get('THUMBS_UP', 0),
        minus_one=counts.get('THUMBS_DOWN', 0),
        laugh=counts.get('LAUGH', 0),
        hooray=counts.get('HOORAY', 0),
        confused=counts.get('CONFUSED', 0),
        heart=counts.get('HEART', 0),
        rocket=counts.get('ROCKET', 0),
        eyes=counts.get('EYES', 0),
    )

    #--- Contributors ---#
    # Author
    contributors_map = {author_login: author_avatar}
    # Assignees
    assignees = issue_data.get('assignees').get('nodes', [])
    for assignee in assignees:
        login = assignee.get('login')
        if login not in contributors_map:
            contributors_map[login] = assignee.get('avatarUrl')
    # Commenters
    comments = issue_data.get('comments').get('nodes', [])
    sorted_comments = sorted(comments, key=lambda x: x.get('createdAt'))
    for comment in sorted_comments:
        commenter = comment.get('author')
        login = commenter.get('login')
        if login not in contributors_map:
            contributors_map[login] = commenter.get('avatarUrl')
    # Insertion order is preserved in Python 3.7+
    contributors_logins = list(contributors_map.keys())
    contributors_avatars = list(contributors_map.values())

    #--- Last commenter ---#
    last_commenter = None
    last_commented_at = None
    if sorted_comments and len(sorted_comments) > 0:
        last_commenter = sorted_comments[-1].get('author', {}).get('login')
        last_commented_at = sorted_comments[-1].get('createdAt')

    #--- Linked branch ---#
    linked_branches_data = issue_data.get('linkedBranches', {}).get('nodes', [])
    linked_branch_name = None
    linked_branch_commit = None
    # If there are multiple linked branches, we take the first one
    for branch in linked_branches_data:
        branch_ref = branch.get('ref', {})
        linked_branch_name = branch_ref.get('name')
        linked_branch_commit = branch_ref.get('target', {}).get('oid')
        break

    #--- Linked PR ---#
    timeline_items = issue_data.get('timelineItems', {}).get('nodes', [])
    linked_pr = None
    linked_pr_state = None
    for item in timeline_items:
        pr_info = item.get('subject', item.get('source', {}))

        # If this is a cross-referenced PR, we check if it Fixes/Closes this issue
        if 'source' in item:
            # Ignore empty cross-references
            if not 'body' in pr_info:
                continue
            # Skip if the PR does not reference this issue
            issue_number = issue_data['number']
            keywords_to_check = [f'fixes #{issue_number}', f'closes #{issue_number}']
            if not any(keyword in pr_info.get('body', '').lower() for keyword in keywords_to_check):
                continue

        # Get linked PR info
        linked_pr = pr_info.get('number')
        linked_pr_state = PrState.OPEN
        if pr_info.get('state') == 'MERGED':
            linked_pr_state = PrState.MERGED
        elif pr_info.get('state') == 'CLOSED':
            linked_pr_state = PrState.CLOSED

    #--- Parse tasks ---#
    body = issue_data.get('body', '')
    completed_tasks = body.count("[x]") + body.count("\\[x\\]") + body.count("[X]") + body.count("\\[X\\]")
    incomplete_tasks = body.count("[ ]") + body.count("\\[ \\]")
    total_tasks = completed_tasks + incomplete_tasks

    #--- Commits/Additions/Deletions ---#
    commit_count = None
    files_changed = None
    additions = None
    deletions = None
    if linked_pr is not None:
        commit_count, files_changed, additions, deletions = get_pr_stats(linked_pr)
    elif linked_branch_name is not None:
        commit_count, files_changed, additions, deletions = get_branch_stats(linked_branch_name)

    #--- Create Issue ---#
    issue = Issue(
        number = issue_data['number'],
        title = title,
        type = issue_type,
        is_pinned = issue_data.get('isPinned', False),
        state = state_enum,
        url = issue_data['url'],
        author = author_login,
        created_at = issue_data['createdAt'],
        labels = parsed_labels,
        contributors = contributors_logins,
        contributors_avatars = contributors_avatars,
        reactions = parsed_reactions,
        completed_tasks = completed_tasks,
        total_tasks = total_tasks,
        comment_count = comment_count,
        last_commenter = last_commenter,
        last_commented_at = last_commented_at,
        linked_branch = linked_branch_name,
        linked_pr = linked_pr,
        linked_pr_state = linked_pr_state,
        commit_count = commit_count,
        files_changed = files_changed,
        additions = additions,
        deletions = deletions,
    )

    return issue

#----- Fetch Status Labels -----#
def fetch_status_labels() -> List[StatusLabel]:
    """
    Fetches the status labels from the repository.

    Returns:
        A list of tuples containing label names, their color, and number of issues with that label.
    """

    query = f"""
    query GetStatusLabels {{
      repository(owner: "{REPO_OWNER}", name: "{REPO_NAME}") {{
        labels(first: 50) {{
          nodes {{
              name
              color
              issues {{
                totalCount
              }}
            }}
          }}
        }}
      }}
    """

    fetched_labels: List[StatusLabel] = []

    logging.info(f"Fetching status labels from {REPO_OWNER}/{REPO_NAME}...")
    try:
        response = requests.post(API_BASE_URL_GRAPHQL, headers=HEADERS_GRAPHQL, json={"query": query})
        response.raise_for_status()
        data = response.json()
        labels_data = data.get('data', {}).get('repository', {}).get('labels', {}).get('nodes', [])
        logging.info(f"Received {len(labels_data)} items from API. Parsing...")

        for label_data in labels_data:
            label_name = label_data.get('name', '')
            label_color = f"#{label_data.get('color', '')}"
            issue_count = label_data.get('issues', {}).get('totalCount', 0)
            if 'status:' in label_name:
                fetched_labels.append(StatusLabel(name=label_name, color=label_color, issue_count=issue_count))

    except requests.exceptions.RequestException as e:
        logging.error(f"Error fetching status labels from GitHub API: {e}")
    except json.JSONDecodeError as e:
         logging.error(f"Error decoding JSON response from GitHub API: {e}")
    except Exception as e:
        logging.error(f"An unexpected error occurred: {e}")

    logging.info(f"Returning {len(fetched_labels)} labels.")
    return fetched_labels

#----- Fetch Issues -----#
def fetch_top_issues(count: int) -> List[Issue]:
    """
    Fetches issues, prioritizing pinned issues, then filling with recent open issues.

    Args:
        count: The total number of issues desired in the final list.

    Returns:
        A list of Issue objects, with pinned issues first, up to 'count' total.
    """

    query = f"""
    query GetIssues {{
      repository(owner: "{REPO_OWNER}", name: "{REPO_NAME}") {{
        issues(first: 50, orderBy: {{field: UPDATED_AT, direction: DESC}}) {{
          nodes {{
              number
              title
              isPinned
              url
              state
              stateReason
              createdAt
              updatedAt
              author {{ login avatarUrl(size: 40) }}
              labels(first: 10) {{ nodes {{ name color }} }}
              assignees(first: 10) {{ nodes {{ login avatarUrl(size: 40) }} }}
              comments(first: 50, orderBy: {{ field: UPDATED_AT, direction: DESC }}) {{
                totalCount
                nodes {{
                  createdAt
                  author {{
                    login
                    avatarUrl(size: 40)
                  }}
                }}
              }}
              reactionGroups {{
                content
                reactors {{
                  totalCount
                }}
              }}
              # Get linked branches
              linkedBranches(first: 10) {{
                nodes {{
                  ref {{
                    name
                    target {{
                      ... on Commit {{
                        oid
                      }}
                    }}
                  }}
                }}
              }}
              # Get linked PRs
              timelineItems(itemTypes: [CONNECTED_EVENT, CROSS_REFERENCED_EVENT], first: 10) {{
                nodes {{
                  ... on ConnectedEvent {{
                    subject {{
                      ... on PullRequest {{
                        number
                        title
                        url
                        state
                        merged
                        author {{
                          login
                        }}
                      }}
                    }}
                  }}
                  ... on CrossReferencedEvent {{
                    source {{
                      ... on PullRequest {{
                        number
                        title
                        url
                        state
                        body # Parse body to make sure it was referenced with Fixes/Closes keyword
                        author {{
                          login
                        }}
                      }}
                    }}
                  }}
                }}
              }}
              # Get body (parse for tasks)
              body
            }}
          }}
        }}
      }}
    """

    fetched_issues: List[Issue] = []

    logging.info(f"Fetching last {count} updated issues from {REPO_OWNER}/{REPO_NAME}...")
    try:
        response = requests.post(API_BASE_URL_GRAPHQL, headers=HEADERS_GRAPHQL, json={"query": query})
        response.raise_for_status()
        data = response.json()
        issues_data = data.get('data', {}).get('repository', {}).get('issues', {}).get('nodes', [])

        if not issues_data:
            logging.info("No open issues found matching the criteria.")
            return fetched_issues

        logging.info(f"Received {len(issues_data)} items from API. Parsing...")

        # Parse pinned issues first
        for issue_data in issues_data:
            if len(fetched_issues) >= count:
                break
            try:
                if not issue_data.get('isPinned', False):
                    continue
                # Parse the raw data into the structured Issue object
                pinned_issue = parse_issue_from_data(issue_data)
                fetched_issues.append(pinned_issue)
            except Exception as parse_error:
                 logging.error(f"Failed to parse pinned issue #{issue_data.get('number')}: {parse_error}", exc_info=True)

        # Parse remaining issues (most recent first)
        for issue_data in issues_data:
            if len(fetched_issues) >= count:
                break
            try:
                # Skip already added pinned issues
                if issue_data.get('isPinned', False):
                    continue
                # Parse the raw data into the structured Issue object
                issue = parse_issue_from_data(issue_data)
                fetched_issues.append(issue)
            except Exception as parse_error:
                 logging.error(f"Failed to parse issue #{issue_data.get('number')}: {parse_error}", exc_info=True)

    except requests.exceptions.RequestException as e:
        logging.error(f"Error fetching issues from GitHub API: {e}")
    except json.JSONDecodeError as e:
         logging.error(f"Error decoding JSON response from GitHub API: {e}")
    except Exception as e:
        logging.error(f"An unexpected error occurred: {e}")

    logging.info(f"Returning {len(fetched_issues)} parsed issues.")
    return fetched_issues

#----- Main -----#
#if __name__ == "__main__":
#    status_labels = fetch_status_labels()
#    for label in status_labels:
#        logging.info(f"LABEL {label}");
#
#    top_issues = fetch_top_issues(10)
#    for issue in top_issues:
#        logging.info(10*"-" + f"ISSUE #{issue.number}" + 10*"-");
#        logging.info(f"ISSUE {issue}");
