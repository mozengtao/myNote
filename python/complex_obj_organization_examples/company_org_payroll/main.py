"""
公司组织架构与工资单 —— 完整可运行示例

本示例重点演示的心智模型原则（对应 complex_obj_organization.md 第十七节 Checklist）：
    1. Builder            —— OrgChartBuilder 逐步 add_department() / add_employee()，
       一步步构建出复杂的组织树，区别于"一次性从 JSON 转换"的 Factory 模式。
    2. 避免 God Object     —— Company 只描述"公司有哪些部门、部门有哪些员工"，
       不直接把发工资、算 KPI、算个税这些逻辑塞进 Company/Employee 里，而是拆到独立的 PayrollService。

运行方式：
    python3 main.py
"""

from __future__ import annotations

from dataclasses import dataclass, field


# ---------------------------------------------------------------------------
# 一、Domain Object：Company -> Department -> Employee 的组合树
# ---------------------------------------------------------------------------

@dataclass
class Employee:
    """一个员工，只描述"是谁、底薪多少、绩效系数"，不知道怎么发工资、怎么算税。"""

    employee_id: str
    name: str
    base_salary: float
    performance_factor: float = 1.0  # 绩效系数：1.0 = 满绩效


class Department:
    """一个部门，拥有若干员工（Has-A），只负责管理"部门里有哪些人"。"""

    def __init__(self, name: str):
        self.name = name
        self.employees: list[Employee] = []

    def add_employee(self, employee: Employee) -> None:
        self.employees.append(employee)

    def headcount(self) -> int:
        return len(self.employees)

    def __repr__(self) -> str:
        return f"<Department {self.name} ({self.headcount()} 人)>"


class Company:
    """
    公司拥有若干部门（Has-A），是整棵组织树的根节点。

    Company 不知道怎么发工资（那是 PayrollService 的事），
    也不知道怎么算 KPI（那是别的 Service 的事）——
    避免把所有逻辑都堆进 Company，变成一个 God Object。
    """

    def __init__(self, name: str):
        self.name = name
        self.departments: dict[str, Department] = {}

    def add_department(self, department: Department) -> None:
        self.departments[department.name] = department

    def get_department(self, name: str) -> Department:
        return self.departments[name]

    def all_employees(self) -> list[Employee]:
        """Hide Structure：对外提供"取出全公司员工"的方法，不用让外部自己遍历 departments。"""
        employees: list[Employee] = []
        for department in self.departments.values():
            employees.extend(department.employees)
        return employees

    def total_headcount(self) -> int:
        return sum(dept.headcount() for dept in self.departments.values())


# ---------------------------------------------------------------------------
# 二、OrgChartBuilder：逐步构建复杂对象图（Builder 模式）
# ---------------------------------------------------------------------------

class OrgChartBuilder:
    """
    Builder 逐步构造组织架构：

        builder.add_department("研发部")
        builder.add_employee("研发部", ...)
        builder.add_employee("研发部", ...)
        builder.add_department("销售部")
        ...
        company = builder.build()

    与 Factory 的区别：Factory 通常是"一次性"把原始数据转成对象（例如从一份完整 JSON），
    而 Builder 更适合"分步骤、逐条录入"的场景（比如 HR 系统里一个个部门、一个个员工地录入）。
    """

    def __init__(self, company_name: str):
        self._company = Company(name=company_name)

    def add_department(self, department_name: str) -> "OrgChartBuilder":
        self._company.add_department(Department(name=department_name))
        return self  # 支持链式调用

    def add_employee(
        self,
        department_name: str,
        employee_id: str,
        name: str,
        base_salary: float,
        performance_factor: float = 1.0,
    ) -> "OrgChartBuilder":
        department = self._company.get_department(department_name)
        department.add_employee(
            Employee(
                employee_id=employee_id,
                name=name,
                base_salary=base_salary,
                performance_factor=performance_factor,
            )
        )
        return self

    def build(self) -> Company:
        """构建完成，把最终的 Company 对象图交给调用方。"""
        return self._company


# ---------------------------------------------------------------------------
# 三、PayrollService：独立于 Company/Employee 的业务逻辑，避免 God Object
# ---------------------------------------------------------------------------

@dataclass
class PayStub:
    """一张工资单，是 PayrollService 消费 Employee 对象后产出的结果。"""

    employee_id: str
    name: str
    base_salary: float
    bonus: float
    gross_pay: float = field(init=False)

    def __post_init__(self) -> None:
        self.gross_pay = self.base_salary + self.bonus


class PayrollService:
    """
    发工资的逻辑单独放在这里，而不是塞进 Employee 或 Company：

    - Employee 不需要知道"绩效系数怎么换算成奖金"
    - Company 不需要知道"工资单长什么样"

    这样以后如果发工资规则变了（比如改个税起征点、改绩效系数换算方式），
    只需要改 PayrollService，不用动 Company/Department/Employee 的定义。
    """

    def __init__(self, bonus_rate: float = 0.2):
        self._bonus_rate = bonus_rate

    def calculate_pay_stub(self, employee: Employee) -> PayStub:
        bonus = employee.base_salary * self._bonus_rate * employee.performance_factor
        return PayStub(
            employee_id=employee.employee_id,
            name=employee.name,
            base_salary=employee.base_salary,
            bonus=bonus,
        )

    def run_payroll(self, company: Company) -> list[PayStub]:
        return [self.calculate_pay_stub(emp) for emp in company.all_employees()]


# ---------------------------------------------------------------------------
# 四、演示
# ---------------------------------------------------------------------------

def main() -> None:
    builder = OrgChartBuilder(company_name="心智科技有限公司")

    # 逐步构建组织架构：一个部门一个部门地加，一个员工一个员工地加
    (
        builder
        .add_department("研发部")
        .add_employee("研发部", employee_id="E001", name="Alice", base_salary=20000, performance_factor=1.2)
        .add_employee("研发部", employee_id="E002", name="Bob", base_salary=18000, performance_factor=0.9)
        .add_department("销售部")
        .add_employee("销售部", employee_id="E003", name="Carol", base_salary=15000, performance_factor=1.5)
    )

    company = builder.build()

    print("=== 组织架构 ===")
    for department in company.departments.values():
        print(department)
        for emp in department.employees:
            print(f"  - {emp.name}（底薪 {emp.base_salary}，绩效 {emp.performance_factor}）")

    print(f"\n公司总人数：{company.total_headcount()}")

    payroll = PayrollService(bonus_rate=0.2)
    pay_stubs = payroll.run_payroll(company)

    print("\n=== 本月工资单 ===")
    for stub in pay_stubs:
        print(
            f"- {stub.name}: 底薪 {stub.base_salary:.2f} + 奖金 {stub.bonus:.2f}"
            f" = 应发 {stub.gross_pay:.2f}"
        )


if __name__ == "__main__":
    main()
