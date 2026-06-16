<div align="center">

<img src=".github/assets/fn-logo-dark-v2.svg" alt="ForgeNest Inc." width="260"/>

# PROJECT_NAME

One-line description.

</div>

---

## Setup

```bash
git clone https://github.com/forgenest-inc/PROJECT_NAME.git
cd PROJECT_NAME
pip install -r requirements.txt
pytest
```

## Branches

| Branch | Purpose |
|---|---|
| `main` | Production. |
| `release` | Release prep, branched from `develop`. |
| `develop` | Integration. Features merge here first. |
| `feature/*` | New work, branched from `develop`. |
| `hotfix/*` | Emergency fixes, branched from `main`. |

## Automation

- **CI** runs the  test suite on every push and pull request to `main`, `release`, or `develop`.
- **CI On Demand** runs the test suite on any other branch when the commit message contains `[ci]`.
- **Issue Tracker** scans new commits for task-style comments in source code (`TODO`, `FIXME`, `HACK`) and opens a GitHub issue assigned to the commit author. The issue closes automatically when the comment is removed.

## License

Proprietary — © 2025 ForgeNest Inc. See [LICENSE](LICENSE).
