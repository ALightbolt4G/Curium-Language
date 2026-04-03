## Calculator SPA (CM Example)

This example demonstrates a minimal full-stack setup:

- **Backend**: `src/main.curium` starts the CM HTTP server and serves `public_html/`.
- **Frontend**: `public_html/` contains a small calculator SPA.

### Run

From the repo root:

1. Build the compiler once:

```
cmake -S . -B build
cmake --build build --config Release
```

2. Compile and run the example:

```
./cm.exe run examples/calculator_spa/src/main.curium -o calc_app
```

Then open:

- `http://localhost:8080`

