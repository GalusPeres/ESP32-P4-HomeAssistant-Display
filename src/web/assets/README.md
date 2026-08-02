# Admin WebUI assets

Edit `admin.css` and `admin.js`, then regenerate the firmware includes:

```powershell
node tools/generate-web-assets.mjs
```

The generated gzip arrays stay in the repository so Arduino IDE builds do not
need a pre-build hook. Local scripted builds and CI verify them with `--check`.
