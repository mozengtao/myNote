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
```