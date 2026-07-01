"""
银行账户转账 —— 完整可运行示例

本示例重点演示的心智模型原则（对应 complex_obj_organization.md 第十七节 Checklist）：
    1. Encapsulation —— Account 的余额只能通过 deposit() / withdraw() 修改，
       禁止外部直接 account.balance = xxx，避免绕过业务规则（例如透支检查）。
    2. Service 无状态 —— TransferService 不持有任何账户数据，只负责"给定两个账户对象，执行一次转账业务规则"，
       账户对象本身才是在系统中流动、携带状态的东西。

运行方式：
    python3 main.py
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime


class InsufficientFundsError(Exception):
    """业务异常：余额不足。用专门的异常类型，而不是返回 -1 / None 这种模糊信号。"""


# ---------------------------------------------------------------------------
# 一、Domain Object：Account 自己管理自己的余额
# ---------------------------------------------------------------------------

class Account:
    """
    一个银行账户。

    余额是私有状态（`_balance`），只能通过 deposit() / withdraw() 变更，
    这两个方法内部负责校验业务规则（例如不能透支），
    外部代码永远不应该、也没有办法直接给 balance 赋值。
    """

    def __init__(self, account_id: str, owner: str, opening_balance: float = 0.0):
        self.account_id = account_id
        self.owner = owner
        self._balance = opening_balance
        # 每个账户自己维护流水记录，而不是让 Bank 或 Service 拿一个全局列表记账
        self.transactions: list["Transaction"] = []

    @property
    def balance(self) -> float:
        return self._balance

    def deposit(self, amount: float, note: str = "") -> None:
        if amount <= 0:
            raise ValueError("存款金额必须为正数")
        self._balance += amount
        self.transactions.append(Transaction(kind="deposit", amount=amount, note=note))

    def withdraw(self, amount: float, note: str = "") -> None:
        if amount <= 0:
            raise ValueError("取款金额必须为正数")
        if amount > self._balance:
            raise InsufficientFundsError(
                f"账户 {self.account_id} 余额不足：当前 {self._balance}，尝试取出 {amount}"
            )
        self._balance -= amount
        self.transactions.append(Transaction(kind="withdraw", amount=amount, note=note))

    def __repr__(self) -> str:
        return f"<Account {self.account_id} ({self.owner}) balance={self._balance:.2f}>"


@dataclass
class Transaction:
    """一条流水记录，只描述"发生了什么"，不包含任何业务判断逻辑。"""

    kind: str
    amount: float
    note: str = ""
    at: datetime = field(default_factory=datetime.now)


# ---------------------------------------------------------------------------
# 二、Bank：Repository 角色，管理账户集合的生命周期与查找
# ---------------------------------------------------------------------------

class Bank:
    """
    Bank 在这里承担 Repository 的职责：负责账户的创建、查找，
    而不负责具体的转账业务规则（那是 TransferService 的事）。
    """

    def __init__(self, name: str):
        self.name = name
        self._accounts: dict[str, Account] = {}

    def open_account(self, account_id: str, owner: str, opening_balance: float = 0.0) -> Account:
        if account_id in self._accounts:
            raise ValueError(f"账户 {account_id} 已存在")
        account = Account(account_id=account_id, owner=owner, opening_balance=opening_balance)
        self._accounts[account_id] = account
        return account

    def find_account(self, account_id: str) -> Account:
        """Hide Structure：调用方只传 account_id，不用知道账户存在 dict 里。"""
        return self._accounts[account_id]

    def total_assets(self) -> float:
        return sum(account.balance for account in self._accounts.values())


# ---------------------------------------------------------------------------
# 三、TransferService：无状态的业务服务，只消费 Account 对象
# ---------------------------------------------------------------------------

class TransferService:
    """
    TransferService 不持有任何账户引用，也没有 __init__ 里存状态；
    每次调用 transfer() 时，账户对象从外部"流入"，处理完之后状态变化保留在账户对象自身上，
    Service 本身用完即可丢弃、可以被任意多个请求并发复用。
    """

    def transfer(self, from_account: Account, to_account: Account, amount: float) -> None:
        if from_account.account_id == to_account.account_id:
            raise ValueError("转出账户和转入账户不能是同一个账户")

        note = f"转账至 {to_account.account_id}"
        from_account.withdraw(amount, note=note)
        try:
            to_account.deposit(amount, note=f"来自 {from_account.account_id} 的转账")
        except Exception:
            # 转入失败时把已扣的钱补回去，保证账户对象整体状态一致（业务规则封装在 Service 内）
            from_account.deposit(amount, note="转账失败回滚")
            raise


# ---------------------------------------------------------------------------
# 四、演示
# ---------------------------------------------------------------------------

def main() -> None:
    bank = Bank(name="心智银行")

    alice = bank.open_account("A001", "Alice", opening_balance=1000)
    bob = bank.open_account("A002", "Bob", opening_balance=200)

    print("=== 初始账户状态 ===")
    print(alice)
    print(bob)

    transfer_service = TransferService()

    print("\n=== Alice 转账 300 给 Bob ===")
    transfer_service.transfer(from_account=alice, to_account=bob, amount=300)
    print(bank.find_account("A001"))
    print(bank.find_account("A002"))

    print("\n=== Bob 尝试转账 10000 给 Alice（余额不足） ===")
    try:
        transfer_service.transfer(from_account=bob, to_account=alice, amount=10000)
    except InsufficientFundsError as exc:
        print(f"转账被拒绝：{exc}")

    # 封装：外部无法绕过 withdraw/deposit 直接改余额
    print("\n=== 尝试直接修改余额会怎样 ===")
    try:
        bob.balance = 999999  # type: ignore[misc]  # balance 是只读 property，直接赋值会报错
    except AttributeError as exc:
        print(f"被拒绝：{exc}")

    print(f"\n全行总资产：{bank.total_assets():.2f}")

    print("\n=== Alice 的流水记录 ===")
    for tx in alice.transactions:
        print(f"- {tx.kind} {tx.amount:.2f} ({tx.note})")


if __name__ == "__main__":
    main()
