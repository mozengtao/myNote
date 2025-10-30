What is Nomad?  
> Nomad is a flexible scheduler and workload orchestrator that enables you to deploy and manage any application across on-premise and cloud infrastructure at scale.  

[**Introduction to Nomad**](https://developer.hashicorp.com/nomad/tutorials/get-started/gs-overview)  
[Tutorials](https://developer.hashicorp.com/nomad/tutorials)  
[Consul](https://developer.hashicorp.com/consul)  
[From Zero to WOW!](https://medium.com/hashicorp-engineering/hashicorp-nomad-from-zero-to-wow-1615345aa539)  
[Nomad Commands](https://developer.hashicorp.com/nomad/docs/commands)  
[Nomad job specification](https://developer.hashicorp.com/nomad/docs/job-specification)  
[HCL2](https://developer.hashicorp.com/nomad/docs/job-specification/hcl2)  
> Nomad uses the Hashicorp Configuration Language - HCL - designed to allow concise descriptions of the required steps to get to a job file  
[**Nomad Agent Configuration**](https://developer.hashicorp.com/nomad/docs/configuration)  
[Mastering HashiCorp Nomad: A Comprehensive Guide for Deploying and Managing Workloads](https://medium.com/@williamwarley/mastering-hashicorp-nomad-a-comprehensive-guide-for-deploying-and-managing-workloads-aa8720c2620b)  
[]()  
[]()  
[]()  
[]()  
[]()  


## key concepts
```
Job = overall definition
Task Group = deployable unit (can have multiple tasks)
Task = single process/container
Allocation = scheduled running instance of a group
Node = machine where allocations run

A Job is the top-level specification, it describes what you want to run (containers, binaries, services) and how they should run.
A Task Group is a logical group of tasks within a job, Nomad schedules task groups as a unit on a single node.
A Task is the actual workload inside a task group, It defines the driver (like Docker, exec, Java) and what to run.
An Allocation is a running instance of a task group on a specific node, It’s created when Nomad schedules a task group onto a node
A Node is a machine (VM or physical) that runs the Nomad client agent, It provides resources where allocations run.

            Job
            └─ Task Group (count=3)
               ├─ Task A
               └─ Task B
    ┌────────────┬────────────┬────────────┐
 Allocation 1  Allocation 2  Allocation 3
      on Node X     on Node Y     on Node Z



                ┌────────────────────┐
                │        JOB         │
                │  (Deployment plan) │
                └─────────┬──────────┘
                          │
               ┌──────────┴──────────┐
         ┌─────▼─────┐         ┌─────▼─────┐
         │ Task Group│         │ Task Group│
         │  (Unit)   │         │  (Unit)   │
         └─────┬─────┘         └─────┬─────┘
               │                     │
      ┌────────┴───────┐      ┌──────┴────────┐
┌─────▼─────┐ ┌─────▼─────┐   ┌─────▼─────┐ ┌─────▼─────┐
│  Task A   │ │  Task B   │   │  Task C   │ │  Task D   │
│(Process)  │ │(Process)  │   │(Process)  │ │(Process)  │
└───────────┘ └───────────┘   └───────────┘ └───────────┘

   ↓ Scheduler creates Allocations (one per group instance)

        Allocation #1   Allocation #2   Allocation #3
           (running copies of Task Group on Nodes)

             Node X          Node Y          Node Z
         ┌────────────┐  ┌────────────┐  ┌────────────┐
         │ Allocation │  │ Allocation │  │ Allocation │
         │   (Group)  │  │   (Group)  │  │   (Group)  │
         └────────────┘  └────────────┘  └────────────┘

“A job defines groups, each group has tasks, Nomad makes allocations of groups onto nodes.”
```