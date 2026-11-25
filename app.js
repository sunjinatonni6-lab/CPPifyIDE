(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const source = $("source"), output = $("output");
  let wasmModule = null, currentRun = 0;

  const examples = {
    hello: 'print "Hello, World!"\nprint "Welcome to Cppify"',
    input: 'print "What is your name?"\ninput name\nprint "Hello, " + name + "!"',
    math: '// Arithmetic and power operators\nlet a = 20\nlet b = 5\nprint "Addition: " + (a + b)\nprint "Subtraction: " + (a - b)\nprint "Multiplication: " + (a * b)\nprint "Division: " + (a / b)\nprint "Modulus: " + (a % b)\nprint "Power: " + (a ^ 2)\nlet count = 3\ncount++\nprint "After increment: " + count\ncount--\nprint "After decrement: " + count',
    variables: 'let name = "Cppify"\nlet version = 1\nprint "Project: " + name\nprint "Version: " + version\nage = 26\nprint age'
  };

  function updateLines() {
    const count = Math.max(1, source.value.split("\n").length);
    $("lineNumbers").innerHTML = Array.from({ length: count }, (_, i) => i + 1).join("<br>");
  }
  function printLine(value, kind = "output-line") {
    const div = document.createElement("div");
    div.className = kind;
    div.textContent = String(value);
    output.appendChild(div);
  }
  function clearOutput() {
    output.innerHTML = "";
    $("runStatus").textContent = "Ready";
  }

  // These are host callbacks only. All tokenizing, parsing, expression evaluation,
  // variable storage and execution happen in the C++ WebAssembly module.
  globalThis.cppifyPrint = (text) => {
    const isError = String(text).startsWith("✕");
    const isSuccess = String(text).startsWith("✓");
    printLine(text, isError ? "error" : isSuccess ? "success" : "output-line");
    if (isError) $("runStatus").textContent = "Error";
    else if (isSuccess) $("runStatus").textContent = "Completed";
  };
  globalThis.cppifyInput = (name) => window.prompt(`Enter a value for "${name}":`, "") ?? "";

  source.addEventListener("input", updateLines);
  source.addEventListener("scroll", () => { $("lineNumbers").scrollTop = source.scrollTop; });
  source.addEventListener("keydown", (e) => {
    if (e.key === "Tab") {
      e.preventDefault();
      source.setRangeText("  ", source.selectionStart, source.selectionEnd, "end");
      updateLines();
    }
    if (e.ctrlKey && e.key === "Enter") { e.preventDefault(); runProgram(); }
  });
  $("examples").addEventListener("change", (e) => {
    if (examples[e.target.value]) { source.value = examples[e.target.value]; updateLines(); }
    e.target.value = "";
  });
  $("clearBtn").onclick = () => { source.value = ""; updateLines(); source.focus(); };
  $("resetBtn").onclick = () => { source.value = examples.hello; updateLines(); clearOutput(); };
  $("terminalClear").onclick = clearOutput;
  $("runBtn").onclick = runProgram;

  async function runProgram() {
    if (!wasmModule) {
      clearOutput();
      printLine("C++ WebAssembly engine is still loading. Please wait.", "system");
      return;
    }
    const runId = ++currentRun;
    clearOutput();
    $("runStatus").textContent = "Running";
    $("runBtn").disabled = true;
    printLine("▶ Compiling and running with the C++ WebAssembly engine...", "system");
    try {
      wasmModule.ccall("runTinyBasic", null, ["string"], [source.value]);
      if (runId === currentRun && $("runStatus").textContent === "Running") {
        $("runStatus").textContent = "Completed";
      }
    } catch (err) {
      printLine("✕ WebAssembly runtime error: " + (err?.message || String(err)), "error");
      $("runStatus").textContent = "Error";
    } finally {
      if (runId === currentRun) $("runBtn").disabled = false;
    }
  }

  updateLines();
  if (typeof createTinyBasicModule !== "function") {
    $("engineStatus").textContent = "C++ WASM build required";
    $("engineStatus").classList.add("engine-error");
    printLine("C++ WebAssembly files are missing. Run build-wasm.sh with Emscripten to generate tinybasic.js and tinybasic.wasm.", "error");
    return;
  }
  createTinyBasicModule().then((module) => {
    wasmModule = module;
    $("engineStatus").textContent = "C++ WebAssembly Ready";
    $("engineStatus").classList.remove("engine-error");
    document.querySelectorAll(".ready-pill").forEach((el) => { el.textContent = "C++ WASM READY"; });
    $("panelTitle").textContent = "C++ TinyBasic VM · program output";
    printLine("✓ C++ WebAssembly engine loaded. Ready to run.", "system");
  }).catch((err) => {
    $("engineStatus").textContent = "WASM load failed";
    $("engineStatus").classList.add("engine-error");
    printLine("✕ Could not load the C++ WebAssembly engine: " + (err?.message || String(err)), "error");
  });
})();
