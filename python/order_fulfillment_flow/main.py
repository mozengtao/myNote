"""
入口文件：组装各层依赖，运行一次完整的"订单履约"对象流水线。

运行方式（在 order_fulfillment_flow/ 目录下）：

    python main.py
"""

from repositories.customer_repository import CustomerRepository
from repositories.product_repository import ProductRepository
from repositories.order_repository import OrderRepository
from services.inventory_service import InventoryService
from services.payment_service import PaymentService
from services.shipping_service import ShippingService
from services.report_service import ReportService
from workflows.order_fulfillment_workflow import OrderFulfillmentWorkflow


def main():
    customer_repo = CustomerRepository()
    product_repo = ProductRepository()
    order_repo = OrderRepository(customer_repo, product_repo)

    workflow = OrderFulfillmentWorkflow(
        order_repo=order_repo,
        inventory_service=InventoryService(),
        payment_service=PaymentService(),
        shipping_service=ShippingService(),
        report_service=ReportService(),
    )

    print("=" * 60)
    print("开始执行订单履约 Workflow ...")
    print("=" * 60)

    workflow.run()


if __name__ == "__main__":
    main()
