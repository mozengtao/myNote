[Software Architecture Patterns](https://github.com/chapin666/books/tree/master/architecture)  
[The Onion Architecture](https://jeffreypalermo.com/2008/07/the-onion-architecture-part-1/)  
[]()  
[]()  
[The multifaceted issues of software development](https://www.spaceteams.de/en/insights/the-multifaceted-issues-of-software-development)  
[Clean Architecture: A Deep Dive into Structured Software Design](https://www.spaceteams.de/en/insights/clean-architecture-a-deep-dive-into-structured-software-design)  
[How Clean Architecture Solves Common Software Development Challenges](https://www.spaceteams.de/en/insights/how-clean-architecture-solves-common-software-development-challenges)  

## Clean Architecture
[The Clean Architecture I Wish Someone Had Explained to Me](https://medium.com/@rafael-22/the-clean-architecture-i-wish-someone-had-explained-to-me-dcc1572dbeac)  

[The Clean Architecture](./assets/TheCleanArchitecture.png)  

[linux clean architecture analysis](./linux/clean_architecture_analysis.md)  
[linux clean architecture reference](./linux/clean_architecture_reference.md)  

[linux clean architecture analysis](./linux/clean_architecture_analysis2.md)  
[linux clean architecture reference](./linux/clean_architecture_appendix.md)  

[linux clean architecture analysis overview](./linux/linux_clean_architecture_analysis_overview.md)  
[linux clean architecture analysis part1](./linux/linux_clean_architecture_analysis_part1.md)  
[linux clean architecture analysis part2](./linux/linux_clean_architecture_analysis_part2.md)  


整洁架构的核心在于区分"变化"与"不变"
Dependency direction: the higher you are in the software stack, the more prone to change you are, Each layer only knows the layer immediately inside it. In other words, dependencies always point toward the center, such as `UI → ViewModel → UseCase → Repository → API`

execution flow ≠ dependency flow:
The execution path can run through all the layers, but code dependencies must always point inward, toward the domain. The inner layers can call the outer ones — but they must not depend on them.

[execution flow ≠ dependency flow](./assets/ExecutionFlow_vs_DependencyFlow.png)  

> "High-level modules should not depend on low-level implementations. Both should depend on abstractions."

Clean Architecture organizes the system into layers with clearly defined responsibilities, where dependencies always point toward the less volatile center. Business rules become a plugin: stable, testable, and isolated; everything else — UI, database, APIs — simply connects to it, without coupling.