![mcproxy](https://github.com/igricrbx/mc-proxy/assets/114947694/444624eb-bcce-44cb-845e-89cb8a42405d)

# MC-Proxy

MC-Proxy is a simple and lightweight network proxy for managing Minecraft servers, written in pure C.

## Features

- Lightweight and efficient
- Handles multiple connections simultaneously
- Logs connections and errors
- Resolves hostnames

## Configuration

The `servers.conf` file is used to configure the network proxy. Each line in the file represents a server configuration. The format of each line is as follows:

```properties
#server FQDN / IP               destination
survival.domain.example         10.0.1.123:5001
creative.domain.example         10.0.1.123:5002
wynncraft.domain.example        wynncraft.com
*.domain.example                2b2t.org
10.0.0.1.123                    10.0.1.123:5003
```

- `server FQDN / IP`: This is the Fully Qualified Domain Name (FQDN) or IP address that the proxy will listen for. You can use a wildcard (*) to match any subdomain. For example, ``*.domain.example`` will match any subdomain of ``domain.example``.

- `destination`: This is the IP address and port number that the proxy will forward the traffic to. The format is ``IP:Port``, if the port is not provided, ``25565`` is going be used as a default port.

To add a new server, simply add a new line to the `servers.conf` file with the server FQDN and the destination IP and port. To remove a server entry, simply delete its line from the file.

An example can be seen in [servers.conf](/servers.conf) file.

## Getting Started

### Prerequisites

- GCC or any C compiler
- Make

### Building

To build the project, run the following command in the project root directory:

```bash
make
```

This will compile the source files and generate the executable.

### Running

To run the server, use the following command:

```bash
./bin/proxy
```

## Contributing

Contributions are welcome. Please fork the repository and create a pull request with your changes.
