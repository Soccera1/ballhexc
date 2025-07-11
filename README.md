# AI Model C-Code Generation Test: Ball in Hexagon

This repository contains the results of a test where different AI models were tasked with generating a C program based on a specific prompt.

## The Prompt

The following prompt was given to each AI model:

```
Create an xlib + C application where a ball is inside a rotating hexagon. The ball is affected by Earth’s gravity and friction from the hexagon walls. The bouncing must appear realistic.
```

The full prompt is available in the `prompt` file.

## The Models and Their Output

Each C file in this repository is the direct output from a different AI model for the given prompt. This was done to compare their reasoning and code generation capabilities.

The included implementations are from the following models:

*   `c4srballhex.c`: Claude 4 Sonnet Reasoning
*   `g2.5-proballhex.c`: Gemini 2.5-pro
*   `g4ballhex.c`: Grok 4
*   `l4mballhex.c`: Llama 4 Maverick
*   `o4mballhex.c`: o4-mini
*   `qwq32ballhex.c`: qwq-32b

## Building

The project includes a `Makefile` that simplifies the compilation process. To build all the executables, simply run the `make` command:

```bash
make
```

This will compile all the `.c` files and place the executables in the `bin` directory.

## Running

Once the project is built, you can run any of the executables from the `bin` directory to see the corresponding model's simulation. For example:

```bash
./bin/c4srballhex
```

This will open a window and start the simulation. To close the window, you can typically press the 'q' key or the escape key.

## License

This project is licensed under the GNU General Public License v3.0. See the `LICENSE` file for more details.