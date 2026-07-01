class Customer:
    """真实世界中的"客户"，在系统里始终以对象形式存在。"""

    def __init__(self, customer_id: str, name: str, address: str):
        self.customer_id = customer_id
        self.name = name
        self.address = address

    def __repr__(self):
        return f"Customer({self.customer_id}, {self.name!r})"
