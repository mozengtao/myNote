## 1
```python
import asyncio

async def fetch_data(delay, id):
    print("Fetching data... id:", id)
    await asyncio.sleep(delay)
    print("Data fetched, id:", id)
    return f"Data {id}"

async def main():
    task1 = fetch_data(2, 1)
    task2 = fetch_data(2, 2)

    result1 = await task1
    print(f"Received: {result1}")
    result2 = await task2
    print(f"Received: {result2}")

if __name__ == "__main__":
    asyncio.run(main())

# Output
Fetching data... id: 1
Data fetched, id: 1
Received: Data 1
Fetching data... id: 2
Data fetched, id: 2
Received: Data 2
```
## 2
```python
import asyncio

async def fetch_data(id, sleep_time):
    print(f"Corotine {id} starting to fetch data...")
    await asyncio.sleep(sleep_time)
    return {"id": id, "data": f"Data from coroutine {id}"}

async def main():
    task1 = asyncio.create_task(fetch_data(1, 2))
    task2 = asyncio.create_task(fetch_data(2, 1))

    result1 = await task1
    result2 = await task2

    print(result1, result2)

if __name__ == "__main__":
    asyncio.run(main())

# Output
Corotine 1 starting to fetch data...
Corotine 2 starting to fetch data...
{'id': 1, 'data': 'Data from coroutine 1'} {'id': 2, 'data': 'Data from coroutine 2'}
```
## 3
```python
import asyncio

async def fetch_data(id, sleep_time):
    print(f"Corotine {id} starting to fetch data...")
    await asyncio.sleep(sleep_time)
    return {"id": id, "data": f"Data from coroutine {id}"}

async def main():
    # if 1 of the tasks fails, the other tasks are not cancelled automatically
    results = await asyncio.gather(
        fetch_data(1, 2),
        fetch_data(2, 1)
    )

    for result in results:
        print(f"Received: {result}")

if __name__ == "__main__":
    asyncio.run(main())

# Output
Corotine 1 starting to fetch data...
Corotine 2 starting to fetch data...
Received: {'id': 1, 'data': 'Data from coroutine 1'}
Received: {'id': 2, 'data': 'Data from coroutine 2'}
```
## 4
```python
# <  Python3.11
import asyncio

async def fetch_data(id, sleep_time):
    print(f"Corotine {id} starting to fetch data...")
    await asyncio.sleep(sleep_time)
    return {"id": id, "data": f"Data from coroutine {id}"}

async def main():
    tasks = []
    for i, sleep_time in enumerate([2, 1, 3], start=1):
        task = asyncio.create_task(fetch_data(i, sleep_time))
        tasks.append(task)
    # 等待所有任务执行完成
    results = await asyncio.gather(*tasks)

    for result in results:
        print(f"Received: {result}")

if __name__ == "__main__":
    asyncio.run(main())

# >=  Python3.11
import asyncio

async def fetch_data(id, sleep_time):
    print(f"Corotine {id} starting to fetch data...")
    await asyncio.sleep(sleep_time)
    return {"id": id, "data": f"Data from coroutine {id}"}

async def main():
    tasks = []
    async with asyncio.TaskGroup() as tg:
        for i, sleep_time in enumerate([2, 1, 3], start=1):
            task = tg.create_task(fetch_data(i, sleep_time))
            tasks.append(task)

    results = [task.result() for task in tasks]

    for result in results:
        print(f"Received: {result}")

if __name__ == "__main__":
    asyncio.run(main())
# Output

```
## 5
```python
import asyncio

async def set_future_result(future, value):
    await asyncio.sleep(2)
    future.set_result(value)
    print(f"Future result set to: {value}")

async def main():
    loop = asyncio.get_event_loop()
    future = loop.create_future()

    asyncio.create_task(set_future_result(future, "Hello, World!"))

    result = await future
    print(f"Future result: {result}")

if __name__ == "__main__":
    asyncio.run(main())

# Output
Future result set to: Hello, World!
Future result: Hello, World!
```
## 6
```python
import asyncio

shared_resource = 0

lock = asyncio.Lock()

async def modify_shared_resource():
	global shared_resource
	async with lock:
		# Critical section starts
		print("Modifying shared resource...")
		await asyncio.sleep(1)  # Simulate some work
		shared_resource += 1
		print(f"Shared resource modified to: {shared_resource}")
		# Critical section ends

async def main():
    await asyncio.gather(*(modify_shared_resource() for _ in range(5)))

if __name__ == "__main__":
    asyncio.run(main())

# Output
Modifying shared resource...
Shared resource modified to: 1
Modifying shared resource...
Shared resource modified to: 2
Modifying shared resource...
Shared resource modified to: 3
Modifying shared resource...
Shared resource modified to: 4
Modifying shared resource...
Shared resource modified to: 5
```
## 7
```python
import asyncio

async def access_shared_resource(semaphore, resource_id):
    async with semaphore:
        print(f"Accessing shared resource {resource_id}")
        await asyncio.sleep(1)  # Simulate some work with the shared resource
        print(f"Releasing shared resource {resource_id}")

async def main():
    semaphore = asyncio.Semaphore(2)  # Allow only 2 concurrent accesses
    await asyncio.gather(*(access_shared_resource(semaphore, i) for i in range(5)))

if __name__ == "__main__":
    asyncio.run(main())

# Output
Accessing shared resource 0
Accessing shared resource 1
Releasing shared resource 0
Releasing shared resource 1
Accessing shared resource 2
Accessing shared resource 3
Releasing shared resource 2
Releasing shared resource 3
Accessing shared resource 4
Releasing shared resource 4
```

## 8
```python
import asyncio

async def waiter(event):
    print("Waiting for the event to be set...")
    await event.wait()
    print("Event has been set, continuing execution!")

async def setter(event):
    await asyncio.sleep(2)
    event.set()
    print("Event has been set!")

async def main():
    event = asyncio.Event()
    await asyncio.gather(waiter(event), setter(event))

if __name__ == "__main__":
    asyncio.run(main())

# Output
Waiting for the event to be set...
Event has been set!
Event has been set, continuing execution!
```
