# InterProcessCommunication (IPC)

##  Features

- **Server**
  - Runs on a **ROUTER** socket.
  - Validates an initial **FirstHandshake** from clients which tells the server which client operations will be supported.
  - Routes requests to the appropriate client and executes operations.

- **Client 1**
  - Supports:
    - `ADD` (addition)
    - `MULT` (multiplication)
    - `CONCAT` (string concatenation, errors if result length > 32)

- **Client 2**
  - Supports:
    - `SUB` (subtraction)
    - `DIV` (division, errors on division by zero)
    - `FIND_START` (checks if string starts with substring)

- **Error handling**
  - Explicit error reporting for invalid operations
    (e.g., division by zero, oversized concatenation).

---

## Running with Docker Compose

This project is designed to run **exclusively inside Docker** using **Docker Compose**.

### 1. Build the image
The build step compiles the source code and runs the PyTests.
The resulting image will contain only the essentials:
`ipc.so`, the server, and client executables.

```bash
docker compose build
```

### 2. Start the services
```bash
docker compose up -d
```

### 3. Run the clients
Since the clients are interactive, start them in an attached terminal:

#### Client 1
```bash
docker exec -it ipc-client-base /app/client_1 --address ipc-server --port 24737
```

#### Client 2
```bash
docker exec -it ipc-client-base /app/client_2 --address ipc-server --port 24737
```

---

## Using the Clients

Type `help` inside the client to see the available commands:

```text
Commands:
  block/non-block add a b
  block/non-block sub a b
  block/non-block mult a b
  block/non-block div a b
  block/non-block concat s1 s2
  block/non-block find hay needle
  get <ticket> [nowait | wait <ms>]  (retrieve result for non-blocking ticket)
```

### 🔹 Example: Blocking command
```bash
block add 50 50
```

Output:
```text
Result: Int=100
```

### 🔹 Example: Non-blocking command
```bash
non-block add 50 50
```

Output:
```text
IPC Info: [NOT_FINISHED]
ticket=1477438260486275072
```

Retrieve the result later with:

```bash
get 1477438260486275072 wait 500
```

(where `500` indicates the number of milliseconds to wait for the response)

---

## Tech Stack
- **ZeroMQ** – lightweight messaging and IPC
- **Protocol Buffers** – efficient serialization
- **Docker & Docker Compose** – isolated, reproducible environment

---

## Next Steps

This project was a fun exploration of IPC with ZeroMQ and Protocol Buffers, but there’s plenty of room to grow:

- **Shared memory** – experiment with shared memory for even faster communication.
- **CUDA support** – offload some operations to the GPU for heavy computations.
- **Large-scale data** – extend the system so it can crunch and transfer much bigger datasets efficiently.
- **Non-blocking client main** – adapt the client so it can be run from a `main` loop without blocking, making it more practical for real-world scenarios.

---
