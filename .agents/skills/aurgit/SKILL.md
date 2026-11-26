---
name: aurgit
description: Trigger the GitHub Actions AUR deploy workflow (aur-deploy.yml) to publish packaging/PKGBUILD and packaging/x3d-toggle.install to the Arch User Repository (x3d-toggle).
---

# /aurgit — Deploy x3d-toggle to AUR

When the user invokes `/aurgit`, trigger the `aur-deploy.yml` GitHub Actions workflow on `X3D-Toggle/x3d-toggle-main` to publish the current `main` branch `packaging/PKGBUILD` and `packaging/x3d-toggle.install` to the AUR (`x3d-toggle`).

## Steps

1. Check `git status` in `/home/pyro/Development/X3D-Toggle-Main/x3d-toggle-main` to verify there are no uncommitted changes to `packaging/PKGBUILD` or `packaging/x3d-toggle.install` (if there are, warn the user before triggering so they don't deploy stale files).
2. Read `pkgver` and `pkgrel` from `packaging/PKGBUILD` and confirm the target release tag exists on `origin`.
3. Trigger the GitHub Actions workflow:
   ```bash
   gh workflow run aur-deploy.yml --repo X3D-Toggle/x3d-toggle-main --ref main
   ```
   (Pass `-f updpkgsums=true` if the user asks to recalculate checksums, or `-f commit_message="..."` if they provide a custom message.)
4. Report the workflow run URL / status to the user (`gh run list --workflow aur-deploy.yml --repo X3D-Toggle/x3d-toggle-main --limit 1`).
