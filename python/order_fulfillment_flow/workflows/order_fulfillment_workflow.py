from domain.order import Order
from repositories.order_repository import OrderRepository
from services.inventory_service import InventoryService
from services.payment_service import PaymentService
from services.shipping_service import ShippingService
from services.report_service import ReportService


class OrderFulfillmentWorkflow:
    """
    对象流水线：从 Order 到 Order，Workflow 只负责编排顺序，
    不关心每一步内部是如何跟数据库/支付网关/物流公司打交道的。

        Repository -> Order -> Inventory -> Order -> Payment
                    -> Order -> Shipping -> Order -> Report
    """

    def __init__(
        self,
        order_repo: OrderRepository,
        inventory_service: InventoryService,
        payment_service: PaymentService,
        shipping_service: ShippingService,
        report_service: ReportService,
    ):
        self._order_repo = order_repo
        self._inventory_service = inventory_service
        self._payment_service = payment_service
        self._shipping_service = shipping_service
        self._report_service = report_service

    def run(self) -> list[Order]:
        orders = self._order_repo.load_pending_orders()

        orders = [self._inventory_service.reserve(order) for order in orders]
        orders = [self._payment_service.charge(order) for order in orders]
        orders = [self._shipping_service.arrange(order) for order in orders]

        self._report_service.generate(orders)
        return orders
