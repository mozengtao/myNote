### what is Pub/Sub
Publish/subscribe messaging is a form of **asynchronous** service-to-service communication used in serverless and microservices architectures. In a pub/sub model, any **message** published to a **topic** is immediately received by **all of the subscribers** to the topic. Pub/sub messaging can be used to enable **event-driven** architectures, or to **decouple** applications in order to increase **performance**, **reliability** and **scalability**.

![[Image.png]]

#### Decoupling dimensions
1. **Space decoupling**: Publisher and subscriber do not need to know each other (for example, no exchange of IP address and port).
2. **Time decoupling**: Publisher and subscriber do not need to run at the same time. 
3. **Synchronization decoupling**: Operations on both components do not need to be interrupted during publishing or receiving