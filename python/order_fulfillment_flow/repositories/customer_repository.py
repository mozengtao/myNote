from infrastructure import fake_database
from domain.customer import Customer


class CustomerRepository:
    """
    唯一知道"客户原始数据长什么样"的地方。

    对外只暴露 Customer 对象，业务层完全不需要知道
    背后的数据来源是数据库表、REST API 还是本示例中的内存 dict。
    """

    def __init__(self):
        self._customers = {
            raw["customer_id"]: Customer(
                raw["customer_id"], raw["name"], raw["address"]
            )
            for raw in fake_database.RAW_CUSTOMERS
        }

    def get(self, customer_id: str) -> Customer:
        return self._customers[customer_id]
