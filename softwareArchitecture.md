Software architecture is a high-level blueprint of a software system.
Software design focuses on the low-level structure and components.

- COSI (4 dimentions)
    - C: Communication                  ^
    - O: Organization                   |   Security
    - S: Storage                        |   Privacy
    - I: Implementation                 v   Processes

- C: Communication
    - directly in the code
    - http messages -> REST (JSON)
    - graphQl
    - WebSockets
    - gRPC
    - ...

- O: Organization
    - Monolith
    - Service-oriented athitecture
        - microservices
    - Hexagonal -> ports & adapters
    - Event-driven
    - Pipeline
    - Multitier (Multilayer)
    - Client-Server
    - Peer-to-Peer
    - Blackboard
    - Broker

- S: Storage
    - File systems
    - SQL Database
    - NoSQL
    - Vector DB
    - Key-value store
    - RAM

- I: Implementation