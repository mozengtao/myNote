class Product:
    """真实世界中的"商品"，携带库存状态与价格。"""

    def __init__(self, sku: str, name: str, price: float, stock: int):
        self.sku = sku
        self.name = name
        self.price = price
        self.stock = stock

    def reserve(self, quantity: int) -> bool:
        """从库存中锁定数量，成功返回 True，库存不足返回 False。"""
        if self.stock < quantity:
            return False
        self.stock -= quantity
        return True

    def __repr__(self):
        return f"Product({self.sku}, {self.name!r}, price={self.price}, stock={self.stock})"
