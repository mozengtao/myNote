from infrastructure import fake_database
from domain.product import Product


class ProductRepository:
    """把"商品原始数据"转换为 Product 对象，并作为库存的唯一入口。"""

    def __init__(self):
        self._products = {
            raw["sku"]: Product(raw["sku"], raw["name"], raw["price"], raw["stock"])
            for raw in fake_database.RAW_PRODUCTS
        }

    def get(self, sku: str) -> Product:
        return self._products[sku]
