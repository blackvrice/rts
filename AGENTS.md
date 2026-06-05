# AGENTS.md

## Shared AI Agent Rules

- All AI agents working in this repository must follow this file.
- After completing any code change, review the scope with `git status`, commit the changed files, and push the current branch to the remote repository.
- After any code change, build the project and run the built executable/game before committing.
- If the build or run fails, inspect the errors and fix issues introduced by the change before committing or pushing.
- If the project cannot be run automatically, record the exact command attempted and the blocker in the final response.
- For every completed task, update or create a Markdown development note so future AI agents can understand what changed, why it changed, how it was verified, and any remaining follow-up.
- If there is no feature-specific document, use `DEVELOPMENT_LOG.md` for the Markdown development note and include it in the same commit as the related change.
- When editing code, add short comments for non-obvious logic, complex conditions, coordinate math, gameplay rules, or engine-specific assumptions.
- Do not add comments for obvious getters/setters, self-explanatory one-line code, or code whose intent is already clear from names.
- Do not revert existing changes made by the user or other tools unless explicitly asked.
