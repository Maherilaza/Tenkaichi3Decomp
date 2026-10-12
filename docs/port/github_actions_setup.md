# Quick Setup: GitHub Actions for D3D11 Builds

## Step 1: Push Workflow to Repository

```bash
cd Tenkaichi3Decomp
git add .github/workflows/build-d3d11.yml
git add docs/port/github_actions_d3d11.md
git commit -m "Add GitHub Actions workflow for DirectX 11 builds"
git push origin main
```

## Step 2: Configure Secrets (Optional)

If you want automated disc image downloads:

1. Go to: `Settings` → `Secrets and variables` → `Actions`
2. Click `New repository secret`
3. Name: `DISC_IMAGE_URL`
4. Value: URL to your disc image (private server, signed cloud URL, etc.)

## Step 3: Enable Actions

1. Go to `Actions` tab
2. Enable workflows if prompted

## Step 4: Trigger Build

### Automatic:
- Push to `main` or `d3d11-integration` branch
- Create PR to `main`

### Manual:
1. Go to `Actions` tab
2. Select "Build Windows with DirectX 11"
3. Click `Run workflow`
4. Choose branch
5. Check "Create pre-release" if desired
6. Click `Run workflow`

## Step 5: Download Artifacts

1. Go to `Actions` tab
2. Click on workflow run
3. Scroll to "Artifacts" section
4. Download `Tenkaichi3Decomp-Windows-D3D11-{sha}.zip`

## Step 6: Test

1. Extract artifact
2. Run `Tenkaichi3Decomp-setup.exe` with disc image
3. Set `BT3_GPU_API=d3d11`
4. Launch `Tenkaichi3Decomp.exe`
5. Report results!

## Troubleshooting

**Workflow not appearing?**
- Check `.github/workflows/build-d3d11.yml` exists
- Ensure Actions are enabled in repository settings

**Build fails?**
- Check workflow logs for errors
- Verify all D3D11 files are committed
- Ensure build configuration is correct

**Pre-release not created?**
- Check "Create pre-release" was selected
- Verify `GITHUB_TOKEN` permissions
- Ensure running on main repository (not fork)
