# README

## Overview

This project simulates a parent-child process communication system, where multiple child processes send requests to a shared request buffer, and the parent process handles these requests based on priority. The project is part of the Operating Systems course and demonstrates inter-process communication using shared memory and semaphores.

## Project Description

The project consists of three main parts: the parent process, the child processes, and the shared memory segments used for communication. The parent process reads requests from the children, processes them, and sends responses back.

### Code Description

#### Data Structures
We use three different structs:

1. **childRequest**:
    - Contains three integers: the child's ID, segment number, and line number representing the request.

2. **sharedRequest**:
    - Contains semaphores for sending and receiving requests.
    - A semaphore for printing.
    - Two integers, `in` and `out`, which indicate the buffer positions for inserting and retrieving requests, respectively.
    - An array of `childRequest` structs as the buffer.

3. **sharedResponse**:
    - Contains semaphores for sending and receiving responses.
    - An array representing the response lines for each child.

### Main Functionality

1. **Input Handling**:
    - Reads the file name, number of children, lines per segment, and requests per child from the command-line arguments.

2. **Shared Memory and Semaphore Initialization**:
    - Creates shared memory segments for requests and responses.
    - Initializes semaphores and integers.

3. **File Handling**:
    - Opens the input file and counts all lines to determine the number of segments required.

4. **Child Process Creation**:
    - Uses a loop to fork and create N child processes.

5. **Parent Process**:
    - Handles each request from children based on the producer-consumer protocol.
    - Loads the required segment from the file if it differs from the previous one.
    - Writes operations (segment load or replacement) and times to the parent's log file.
    - Sends responses to children using semaphores.

6. **Child Processes**:
    - Each child opens its own log file.
    - Sends requests to the parent and waits for responses using the producer-consumer protocol.
    - Logs the request, response times, and corresponding lines.
    - Waits for a brief period before terminating.

7. **Cleanup**:
    - The parent process waits for all child processes to complete.
    - Destroys semaphores, closes files, and releases shared memory.

### Commands

#### Compilation and Execution
- **Compile the program**:
    ```bash
    gcc -o ergasia1 ergasia1.c -lpthread
    ```
- **Run the program**:
    ```bash
    ./ergasia1 text.txt 4 5 5
    ```
    This example runs the program with 4 children, 5 lines per child, and 5 requests per child.

## License

This project is licensed under the MIT License.

## Acknowledgements

This project was developed as part of the Operating Systems course in the Department of Informatics and Telecommunications. Special thanks to the course instructors and teaching assistants for their guidance.



