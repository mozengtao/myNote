[Java Documentation](https://docs.oracle.com/en/java/)  


- 参考文档
	- [ELEMENTS OF PROGRAMMING INTERVIEWS in Java](https://inprogrammer.com/wp-content/uploads/2022/01/Adnan-Aziz-Tsung-Hsien-Lee-Amit-Prakash-Elements-of-Programming-Interviews-in-Java_-The-Insiders-Guide-2016-CreateSpace-Independent-Publishing-Platform-libgen.lc_.pdf)

[Java 教程（廖雪峰）](https://liaoxuefeng.com/books/java/introduction/index.html)  
[JavaPark](https://github.com/cunyu1943/JavaPark)  

[Package java.util](https://docs.oracle.com/javase/7/docs/api/java/util/package-summary.html)  
[Java-Projects-Collections](https://github.com/kishanrajput23/Java-Projects-Collections/tree/main)  

[Kotlin Programming: The Big Nerd Ranch Guide Book](https://lmsspada.kemdiktisaintek.go.id/pluginfile.php/751912/course/section/65846/kotlin-programming-the-big-nerd-ranch-guide-1nbsped-013516236x_compress%20%281%29.pdf)  

[Effective Java 3rd Edition](https://kea.nu/files/textbooks/new/Effective%20Java%20%282017%2C%20Addison-Wesley%29.pdf)  


## Jave Editions
1. Java SE: Standard Edition
2. Java EE: Enterprise Edition
3. Java ME: Micro Edition

![Java Editions](./assets/JavaEditions.png)  

## Java Concepts
1. JDK: Java Development Kit
2. JRE: Jave Runtime Environment

![JDK and JRE](./assets/JDK_JRE.png)

- JDK binary commands
	- java
	- javac
	- jar
	- javadoc
	- jdb

(source code)      compile           (byte code)        execute
Hello.java       ----------->        Hello.class    ------------>       (Run on JVM)

```java
// 单行注释

/*
多行
注释
/*

// 特殊的多行注释，以/**开头，以*/结束，如果有多行，每行通常以星号开头，写在类和方法的定义处，用于自动创建文档
/**
 * comments
 */
public class Hello {							// 类名首字母大写
	public static void main(String[] args) {	// class内部可以定义若干方法，方法名首字母小写
		System.out.println("Hello world!");
	}
}

// 变量类型
	// 基本类型
		byte, short, int, long
		float, double
		char
		boolean

		int i1 = -1;
		long n1 = 9000000000000000000000L;

		float f1 = 3.14f
		float f2 = 3.14e38f;
		float f3 = 1.0;	// error: 不带 f 结尾类型是 double, 不能赋值给 float

		double d1 = -1.79e308;
		double d2 = 4.9e-324;

		char zh = '中';

		boolean b1 = true;
		boolean b2 = false;

	// 引用类型

		String s = "hello";

// 常量

	final double PI = 3.14; // final 关键字用来定义常量

// 编译器类型推断

	var sb = new StringBuilder();	// var 用来简化变量类型的书写

	等价于

	StringBuilder sb = new StringBuilder();

// 数组

	// traverse
		// 1
		int[] ns = { 1, 2, 3 };
		for (int i = 0; i < ns.length; i++) {
			System.out.println(ns[i]);
		}

		// 2
		int[] ns = { 1, 2, 3 };
		for (int n : ns) {
			System.out.println(n);
		}

		// 3
		import java.util.Arrays;

		int[] ns = { 1, 2, 3 };
		System.out.println(Arrays.toString(ns));

	// sort

	import java.util.Arrays;

	// 1
	int[] ns = {3, 2, 1};
	Arrays.sort(ns);
	System.out.println(Arrays.toString(ns));
	
	// 2
	String[] ns = { "banana", "apple", "pear" };
	Arrays.sort(ns);
	System.out.println(Arrays.toString(ns));
	
	// 3
	int[][] ns = {
		{ 1, 2, 3 },
		{ 4, 5, 6 },
		{ 7, 8, 9 },
	};
	System.out.println(Arrays.deepToString(ns));

// 面向对象编程
class Person {				// 没有明确写extends的类，编译器会自动加上extends Object. Java只允许一个class继承自一个类，因此，一个类有且仅有一个父类
	private String name;
	private int age;

	// 默认构造方法(如果一个类没有定义构造方法，编译器会自动为我们生成一个默认构造方法，它没有参数，也没有执行语句)
	/*
	public Person {
	}
	*/
	// 注意: 如果自定义了一个构造方法，那么，编译器就不再自动创建默认构造方法
	//      如果既要能使用带参数的构造方法，又想保留不带参数的构造方法，那么只能把两个构造方法都定义出来

	public String getName() {
		return name;	// 没有命名冲突，此时可以省略 this，相当于 this.name
	}

	public String setName(String name) {
		if (name == null || name.isBlank()) {
			throw new IllegalArgumentException("invalid name");
		}
		this.name = name;	// this.name 前面的 this 不可省略，否则为函数中的局部变量 name
	}

	public int getAge() {
		return this.age;
	}

	public void setAge(int age) {
		if (age < 0 || age > 100) {
			throw new IllegalArgumentException("invalid age value");
		}
		this.age = age;
	}
}

// 可变参数
	// 1
	class Group {
		private String[] names;

		public void setNames(String... names) {
			this.names = names;
		}
	}
	Group g = new Group();
	g.setNames("A", "B", "C")
	g.setNames("A")
	g.setNames()	// 可变参数可以保证无法传入 null，因为传入 0 个参数时，实际值是一个空数组而不是 null

	// 2
	class Group {
		private String[] names;

		public void setNames(String[] names) {
			this.names = names;
		}
	}
	Group g = new Group();
	g.setNames(new String[] {"A", "B", "C"});	// 调用者需要自己构造 String[]
	g.setNames(null)	// 传入 null 是合法的，因此可能不符合预期

// 构造方法
	// 默认构造方法
		class Person {
			private String name;
			private int age;

			// 默认构造方法(如果一个类没有定义构造方法，编译器会自动为我们生成一个默认构造方法，它没有参数，也没有执行语句)
			/*
			public Person {
			}
			*/
		}

	// 自定义构造方法
		class Person {
			private String name;
			private int age;

			// 自定义构造方法(如果自定义了一个构造方法，那么，编译器就不再自动创建默认构造方法)
			public Person(String name, int age) {
				this.name = name;
				this.age = age;
			}
			// 注意: 如果自定义了一个构造方法，那么，编译器就不再自动创建默认构造方法
			//      如果既要能使用带参数的构造方法，又想保留不带参数的构造方法，那么只能把两个构造方法都定义出来
		}

	// 同时保留默认构造函数和自定义函数(同时定义两个构造方法)
		class Person {
			private String name;	// null
			private int age;		// 0

			public Person() {	// 定义构造方法1
			}

			public Person(String name, int age) {	// 定义构造方法2
				this.name = name;
				this.age = age;
			}
		}

	// 对象实例的初始化顺序
		class Person {
			private String name = "Unamed";
			private int age = 10;

			public Person(String name, int age) {
				this.name = name;
				this.age = age;
			}
		}

		// step 1: 初始化字段
		// step 2: 执行构造方法的代码进行初始化

	// 多个构造方法
		class Person {
			private String name;	// null
			private int age;		// 0

			public Person(String name, int age) {
				this.name = name;
				this.age = age;
			}

			public Person(String name) {	// 调用构造方法 Person(String, int)
				this(name, 18);
			}

			public Person() {
				this("Unamed");		// 调用构造方法 Person(String)
			}
		}

// 方法重载
	class Hello {
		public void hello() {
			System.out.println("Hello, world!");
		}

		public void hello(String name) {
			System.out.println("Hello, " + name + "!");
		}

		public void hello(String name, int age) {
			if (age < 18) {
				System.out.println("Hi, " + name + "!");
			} else {
				System.out.println("Hello, " + name + "!");

			}
		}
	}
	// 注意：方法重载的返回值类型通常都是相同的

// 继承
	class Person {
		protected String name;	// 子类无法访问父类的 private 字段或者 private 方法, protected 修饰的字段可以被子类访问 
		protected int age;

		public String getName() {...}
		public void setName(String name) {...}
		public int getAge() {...}
		public void setAge(int age) {...}
	}

	class Student extends Person {
		private int score;

		public Student(String name, int age, int score) {
								// 当子类没有明确调用父类的构造方法时，编译器会自动调用父类的构造方法，即 super()
			super(name, age);	// 如果父类没有默认的构造方法，子类必须显式调用 super() 并给出参数以便让编译器定位到父类的合适的构造方法
			this.score = score;
		}

		public int getScore() { ... }
		public int setScore(int score) { ... }

		public String hello() {
			return "Hello, " + super.name;	// 子类引用父类的字段
		}
	}

// 阻止继承
	public sealed class Shape permits Rect, Circle, Triangle {	// sealed 类 Shape 只允许 指定的 Rect, Circle, Triangle 这 3 个类继承它
		...
	}

	public final class Rect extends Shape { ... }

// 向上转型
	class Person { ... }
	class Student extends Person { ... }

	Person p = new Student();	// upcasting(向上转型): 把一个子类类型转变为父类类型的赋值

// 向下转型 (downcasting: 把一个父类类型强制转型为子类类型)
	class Person { ... }
	class Student extends Person { ... }

	Person p1 = new Student(); // upcasting, ok
	Person p2 = new Person();
	Student s1 = (Student) p1; // downcasting, ok (p1 确实指向 Student 实例)
	Student s2 = (Student) p2; // runtime error! ClassCastException!

	// instanceof: 判断一个变量所指向的实例是否是指定类型，或者这个类型的子类, 如果一个引用变量为null，那么对任何instanceof的判断都为false
	Person p = new Person();
	System.out.println(p instanceof Person); // true
	System.out.println(p instanceof Student); // false

	Student s = new Student();
	System.out.println(s instanceof Person); // true
	System.out.println(s instanceof Student); // true

	Student n = null;
	System.out.println(n instanceof Student); // false

	// Java 14开始，判断instanceof后，可以直接转型为指定变量，避免再次强制转型
	Object obj = "hello";
	if (obj instanceof String) {
		String s = (String) obj;
		System.out.println(s.toUpperCase());
	}
	可以改写为
	Object obj = "hello";
	if (obj instanceof String s) {
		System.out.println(s.toUpperCase());
	}

// 多态 Polymorphic
// Override: 子类定义了一个与父类方法签名完全相同的方法
class Person {
	public void run() { ... }
}

class Student extends Person {
									// @Override 不是必需的
	@Override						// @Override 让编译器检查是否进行了覆写，如果方法签名有误编译器会报错
	public void run() { ... }
}

// 多态：针对某个类型的方法调用，其真正执行的方法取决于运行时实际类型的方法，即在运行时动态决定调用的方法

public class Main {
	public static void main(String[] args) {
		// 给一个有普通收入，工资收入和享受国务院特殊津贴的小伙伴算税
		Income[] incomes = new Income[] {
			new Income(3000),
			new Salary(7500),
			new StateCouncilSpecialAllowance(15000)
		}
	}
}

class Income {
	protected double income;

	public double getTax() {
		return income * 0.1;
	}
}

class Salary extends Income {
	@Override
	public double getTax() {
		if (income <= 5000) {
			return 0;
		}
		return (income - 5000) * 0.2;
	}
}

class StateCouncilSpecialAllowance  extends Income {
	@Override
	public double getTax() {
		return 0;
	}
}

// 覆写 Object 方法
toString()
equals()
hashCode()

class Person {
	...

	@Override
	public String toString() {
		return "Person:name = " + name;
	}

	@Override
	public boolean equals(Object o) {
		if (o instanceof Person) {
			Person p = (Person) o;
			return this.name.equals(p.name);
		}
		return false;
	}

	@Override
	public int hashCode() {
		return this.name.hashCode();
	}
}

// super() : 在子类覆写的方法中，调用父类被覆写的方法
class Person {
	...

	public String hello() {
		return "Hello, " + name;
	}
}

class Student extends Person {
	@Override
	public String hello() {
		return super.hello() + "!";	// 调用父类被覆写的方法
	}
}

// final 修饰的方法不能被 Override, 即父类不允许子类对它的某个方法进行覆写
class Person {
	...

	public final String hello() {	// final 关键字 修饰的方法
		return "Hello, " + name;
	}
}

class Student extends Person {
	@Override
	public String hello() {
		return super.hello() + "!";	// comile error : 子类不允许覆写父类的方法
	}
}

// final 修饰的类不能被继承，即一个类不希望任何其他类继承它
final class Person {
    protected String name;
}

class Student extends Person {	// compile error: 不允许继承自Person
}

// final 修饰的字段在初始化后不能被修改，即类的实例字段初始化后不能被修改
class Person {
    public final String name = "Unamed";
}

Person p = new Person();
p.name = "New Name"; // compile error!

// 可以在构造方法中初始化 final 字段
class Person {
	public final String name;

	public Person(String name) {
		this.name = name;
	}
}


// 抽象类
如果一个 class 定义了方法，但没有具体的执行代码逻辑，该方法是抽象方法，抽象方法用 abstract 修饰
因为抽象方法无法执行，因此该类必须声明为抽象类，即使用 abstract 关键字进行修饰
抽象类本身只能用于被继承，即强迫子类实现其定义的抽象方法，否则编译会报错，相当于定义了"规范"

public class Main {
	public static void main(String[] args) {
		Person p = new Student();
		p.run();
	}
}

// abstrct class
abstract class Person {
	public abstract void run();
}

class Student extends Person {
	@Override
	public void run() {
		System.out.println("Student.run");
	}
}

// 接口
如果一个抽象类没有字段，即所有方法全部都是抽象方法，那么可以将该抽象类改写为接口 interface，接口定义的所有方法默认都是 public abstract
当具体的 class 实现一个 interface 时，需要使用 implements 关键字
一个类只能继承自另一个类，不能从多个类继承，但是，一个类可以实现多个 interface
一个 interface 可以使用 extends 继承自另一个 interface, 相当于扩展了接口的方法

// 合理设计interface和abstract class的继承关系，可以充分复用代码, 一般来说，公共逻辑适合放在abstract class中，具体逻辑放到各个子类，而接口层次代表抽象程度
Iterable
            Object
    ^          ^
    |          |
Collection     |
    ^  ^    AbstractCollection
    |  |-------|    ^
    |               |
   List             |
    ^               |
    |--------- AbstractList
                ^       ^
                |       |
            ArrayList LinkedList
使用的时候，实例化的对象永远只能是某个具体的子类，但总是通过接口去引用它，因为接口比抽象类更抽象
List list = new ArrayList();	// 用 List 接口应用具体的子类示例
Collection coll = list;			// 向上转型为 Collection 接口
Iterable it = coll;				// 向上转型为 Iterable 接口

// default 方法
在接口中使用 default 关键字来定义方法，并且提供方法的默认实现
实现类可以选择重写此默认方法，也可以直接使用接口中定义的默认实现

使用 default 方法的目的:
1. 引入 default 方法的主要目的是为了接口向后兼容，避免每次接口发生变化（比如新增方法）时，所有实现该接口的类都必须进行更新
	当你想要给现有的接口添加新方法时，不想破坏已有的代码兼容性，可以通过 default 方法提供一个默认实现
2. 接口扩展：允许在接口中定义一些共同的逻辑，供实现类复用
	当接口中有一些通用的功能，但并非每个实现类都需要这些功能时，可以用 default 方法来定义这些可选功能
3. 提升代码的可维护性
	当多个类有共同的行为逻辑时，可以在接口中通过 default 方法提供默认实现，从而减少代码重复
4. 避免抽象类的限制

// 1
public interface Vehicle {
    void start();

    default void stop() {
        System.out.println("Vehicle stopped.");
    }
}

public class Car implements Vehicle {
    @Override
    public void start() {
        System.out.println("Car started.");
    }
}

// 2
public interface Printer {
    void print(String message);

    default void printUpperCase(String message) {
        System.out.println(message.toUpperCase());
    }
}

public class SimplePrinter implements Printer {
    @Override
    public void print(String message) {
        System.out.println(message);
    }
}

// 3
public interface Logger {
    void log(String message);

    default void logError(String error) {
        log("[ERROR] " + error);
    }
}

public class ConsoleLogger implements Logger {
    @Override
    public void log(String message) {
        System.out.println(message);
    }
}

public class FileLogger implements Logger {
    @Override
    public void log(String message) {
        // 将日志写入文件
    }
}

// 4
public interface Walkable {
    default void walk() {
        System.out.println("Walking");
    }
}

public interface Runnable {
    default void run() {
        System.out.println("Running");
    }
}

public class Athlete implements Walkable, Runnable {
    // Athlete 可以同时继承 walk() 和 run() 的默认实现
}

// 静态字段 和 静态方法
静态字段 和 静态方法 不属于类的实例对象，而属于类本身，因此应该用类名来进行访问
因为静态方法属于class而不属于实例，因此，静态方法内部，无法访问this变量，也无法访问实例字段，它只能访问静态字段
class Person {
	...
	public static int number;

	public static void setNumber(int value) {
		number = value;
	}
	...
}

Person.number = 99;
Person.setNumber(99);

// interface是可以有静态字段的，并且静态字段必须为final类型
public interface Person {
	public static final int MALE = 1;
	public static final int FEMALE = 2;
}

因为interface的字段只能是 public static final 类型, 代码可以简写为

public interface Person {
	int MALE = 1;
	int FEMALE = 2;
}

// package (包)
package 定义了一种命名空间，一个类总是属于某个包，类名只是一种简写，完整的类名是 包名.类名，目的是为了解决类名冲突
包可以是多层接口，例如 java.util，包没有父子关系，com.apache和com.apache.abc是不同的包

// 包作用域
位于同一个包的类，可以访问包作用域的字段和方法。不用public、protected、private修饰的字段和方法就是包作用域
// Person.class
package hello;

public class Person {
	void hello() {	// package scoped
		System.out.println("Hello");
	}
}
// Main.class
package hello;

public class Main {
	public static void main(String[] args) {
		Person p = new Person();
		p.hello();	// Main 和 Person 在同一个包 hello
	}
}

// import 用于在一个 class 中引用其他 class

编写class的时候，编译器会自动帮我们做两个import动作：
	默认自动import当前package的其他class
	默认自动import java.lang.* (java.lang下的例如java.lang.reflect这些包仍需要手动导入)

// 作用域
public
	定义为public的class、interface可以被其他任何类访问

private
	定义为private的field、method无法被其他类访问 (private访问权限被限定在class的内部，而且与方法声明顺序无关。推荐把private方法放到后面，因为public方法定义了类对外提供的功能，阅读代码的时候，应该先关注public方法)
	Java支持嵌套类，如果一个类内部还定义了嵌套类，那么，嵌套类拥有访问private的权限

protected
	protected作用于继承关系。定义为protected的字段和方法可以被子类访问，以及子类的子类

package
	包作用域是指一个类允许访问同一个package的没有public、private修饰的class，以及没有public、protected、private修饰的字段和方法

局部变量

final
	final修饰class可以阻止被继承
	final修饰method可以阻止被子类覆写
	final修饰field可以阻止被重新赋值
	final修饰局部变量可以阻止被重新赋值

// 内部类
定义在另一个类的内部的类称为内部类 (Inner Class)
// 内部类的类型
Inner Class
	Inner Class的作用域在Outer Class内部，所以能访问Outer Class的private字段和方法

Anonymous Class
	class Outer {
		private String name;

		Outer(String name) {
			this.name = name;
		}

		void asyncHello() {
			Runnable r = new Runnable() {	// 定义了一个实现了Runnable接口的匿名类，并且通过new实例化该匿名类，然后转型为Runnable
				@Override
				public void run() {
					System.out.println("Hello, " + Outer.this.name);
				}
			};
			new Thread(r).start();
		}
	}

Static Nested Class

// classpath
classpath是JVM用到的一个环境变量，用来指示JVM如何搜索class

// jar 包
jar 包用来把 package 组织的目录层级，以及各个目录下的所有文件打包成一个 jav 文件，便于备份或者发布给客户，开源工具 Maven 用来方便的创建 jar 包

// class 版本
通常说的Java 8，Java 11，Java 17，是指JDK的版本，也就是JVM的版本，更确切地说，就是java.exe这个程序的版本 (jave --version)。每个版本的JVM，它能执行的class文件版本也不同。例如，Java 11对应的class文件版本是55，而Java 17对应的class文件版本是61

在编写源代码的时候，我们通常会预设一个源码的版本。在编译的时候，如果用--source或--release指定源码版本，则使用指定的源码版本检查语法

运行时使用哪个JDK版本，编译时就尽量使用同一版本的JDK编译源码

// 模块 (Module)
jar只是用于存放class的容器，它并不关心class之间的依赖，而模块(Module)主要是为了解决"依赖"问题

// Java 核心类
// String
String是引用类型，它本身也是一个class

String s1 = "Hello";
等价于
String s1 = new String(new char[] {'H', 'e', 'l', 'l', 'o'});

Java字符串的一个重要特点就是字符串不可变。这种不可变性是通过内部的private final char[]字段，以及没有任何修改char[]的方法实现的

// 比较
String s1 = "hello";
String s2 = "hello";
System.out.println(s1 == s2);	// 比较的是引用
System.out.println(s1.equals(s2));	// 比较的是内容

System.out.println(s1.equalsIgnoreCase(s2));

// 子串
s1.contains(s2);	// contains()方法的参数是CharSequence而不是String，因为CharSequence是String实现的一个接口

"Hello".startsWith("He"); // true
"Hello".endsWith("lo"); // true

// 索引
"Hello".indexOf("l"); // 2
"Hello".lastIndexOf("l"); // 3

// trim/strip
"  \tHello\r\n ".trim(); // "Hello"	(返回了一个新字符串)

"\u3000Hello\u3000".strip(); // "Hello"	(和trim()不同的是，类似中文的空格字符\u3000也会被移除)
" Hello ".stripLeading(); // "Hello "
" Hello ".stripTrailing(); // " Hello"

// 空串 和 空白字符判断
"".isEmpty(); // true，因为字符串长度为0
"  ".isEmpty(); // false，因为字符串长度不为0

"  \n".isBlank(); // true，因为只包含空白字符
" Hello ".isBlank(); // false，因为包含非空白字符

// 替换
String s = "hello";
s.replace('l', 'w'); // "hewwo"，所有字符'l'被替换为'w'
s.replace("ll", "~~"); // "he~~o"，所有子串"ll"被替换为"~~"

String s = "A,,B;C ,D";
s.replaceAll("[\\,\\;\\s]+", ","); // "A,B,C,D"

// 分割
String s = "A,B,C,D";
String[] ss = s.split("\\,"); // {"A", "B", "C", "D"}

// 连接
String[] arr = {"A", "B", "C"};
String s = String.join("***", arr); // "A***B***C"

// 格式化打印
String s = "Hi %s, your score is %d!";
System.out.println(s.formatted("Alice", 80));
System.out.println(String.format("Hi %s, your score is %.2f!", "Bob", 59.5));

// 类型转换
String.valueOf(123); // "123"
String.valueOf(45.67); // "45.67"
String.valueOf(true); // "true"
String.valueOf(new Object()); // 类似java.lang.Object@636be97c

int n1 = Integer.parseInt("123"); // 123
int n2 = Integer.parseInt("ff", 16); // 按十六进制转换，255

boolean b1 = Boolean.parseBoolean("true"); // true
boolean b2 = Boolean.parseBoolean("FALSE"); // false

char[] cs = "Hello".toCharArray(); // String -> char[]
String s = new String(cs); // char[] -> String

// 把字符串转换成其他编码 (转换编码后，就不再是char类型，而是byte类型表示的数组)
byte[] b1 = "Hello".getBytes(); // 按系统默认编码转换，不推荐
byte[] b2 = "Hello".getBytes("UTF-8"); // 按UTF-8编码转换
byte[] b2 = "Hello".getBytes("GBK"); // 按GBK编码转换
byte[] b3 = "Hello".getBytes(StandardCharsets.UTF_8); // 按UTF-8编码转换

// 把已知编码的byte[]转换为String
byte[] b = ...
String s1 = new String(b, "GBK"); // 按GBK转换
String s2 = new String(b, StandardCharsets.UTF_8); // 按UTF-8转换

注意：Java的String和char在内存中总是以Unicode编码表示

// StringBuilder
为了能高效拼接字符串，Java标准库提供了StringBuilder，它是一个可变对象，可以预分配缓冲区，这样，往StringBuilder中新增字符时，不会创建新的临时对象
StringBuilder可以支持链式操作，实现链式操作的关键是返回实例本身

var sb = new StringBuilder(1024);
sb.append("Mr ")
  .append("Bob")
  .append("!")
  .insert(0, "Hello, ");
System.out.println(sb.toString());

// 仿照 StringBuilder 实现支持链式操作的类
public class Main {
	public static void main(String[] args) {
		Adder adder = new Adder();
		adder.add(3)
			 .add(5)
			 .inc()
			 .add(10);
		System.out.println(adder.value());
	}
}

class Adder {
	private int sum = 0;

	public Adder add(int n) {
		sum += n;
		return this;
	}

	public Adder inc() {
		sum ++;
		return this;
	}

	public int value() {
		return sum;
	}
}

// StringJoiner
StringJoiner 专门用来拼接数组的处理

import java.util.StringJoiner;

public class Main {
	public static void main(String[] args) {
		String[] names = {"Bob", "Alice", "Grace"};
		var sj = new StringJoiner(", ", "Hello ", "!");	// 指定开头和结尾
		for (String name : names) {
			sj.add(name);
		}
		System.out.println(sj.toString());
	}
}

// 不需要指定开头和结尾时，使用 String.join() 更方便
String[] names = {"Bob", "Alice", "Grace"};
var s = String.join(", ", names);

// 包装类型
基本类型	对应的引用类型
boolean	   java.lang.Boolean
byte	   java.lang.Byte
short	   java.lang.Short
int		   java.lang.Integer
long	   java.lang.Long
float	   java.lang.Float
double	   java.lang.Double
char	   java.lang.Character


```