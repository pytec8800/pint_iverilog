
# Getting Started with PLS

The project is developed based on icarus iverilog. Special thanks to Stephen Williams (steve@icarus.com).  

PLS is a Verilog simulator, compiles verilog into a much faster optimized model, which executes about 
20-100x faster than interpreted Verilog simulators such as icarus iverilog vvp.

# Performance

PLS performance versus the closed-source Verilog simulators is about [0.5, 10] in typical test.
In decoder cases, it executes about 1-10x over closed-source simulators, average 3.85x.
In uap cases, it executes average 0.72x versus closed-source simulators.

## 1. Install From Source

### 1.1 Compile Time Prerequisites

You need the following softwares to compile PLS from source on a UNIX-like system:

- GNU Make
- CMake
- ISO C++ Compiler
- bison and flex
- gperf 3.0 or later
- bash

### 1.2 Compilation

Unpack the tar-ball and compile the source with the commands:
```bash
  cd pint_iverilog
  sh autoconf.sh
  ./configure
  make -j8
  sudo make install

  cd pint_iverilog/simu
  mkdir build
  cd build
  cmake ..
  make
  sudo make install
```
## 2. Simulation example

For simulation, we use the following design as an example:    
```verilog
  module main;
  initial
    begin
      $display("Hello, World");
      $finish ;
    end
  endmodule
```
1. Arrange for the above program to be in a text file, `hello.v`.
2. Compile the program with the `pls` command:
    ```bash
    pls hello.v
    ```
3. The results of the verilog compile modle is put into the `./a.out` file.
4. execute the compile modle:
    ```bash
    ./a.out
    Hello, World
    ```

## 3. Execute in cpu mode

If your environment is not equipped with a PINT accelerator card and not install the pint_sdk, 
then it will default compile and execute in cpu mode:

    ```bash
    pls hello.v
    ./a.out
    ```
## 4. Execute in pint mode

The PLS verilog compile model is also designed to support parrallel processing, which can further accelerate the simulation.

### 4.1 Local device

If your environment is equipped with a PINT accelerator card and installed the pint_sdk, 
then it will default run the simulation in pint mode, you can also use './a.out cpu_mode' to execute in cpu mode.

    ```bash
    pls hello.v
    ./a.out
    ```

### 4.2 Remote server

If your environment is not equipped with a PINT accelerator card but installed the pint_sdk, 
then it will default compile the pint model, you can also use './a.out cpu_mode' to execute in cpu mode.

    ```bash
    pls hello.v
    ./a.out compile_only
    ```
then you can use the PINT PLS remote server to run the simulation. 
For details, see the PINT PLS Remote Server Manual for RTL Simulation on: http://www.panyi-tech.com/


