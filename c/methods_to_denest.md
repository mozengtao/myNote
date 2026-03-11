## Methods to Denest(减少函数嵌套的方法)
1. Extraction(提取法:把函数的一部分提出出来变成独立的函数)
2. Inversion(反转法:反转提前判断，改用提前判断的方式)

```c
// Before
int calculate(int bottom, int top)
{
	if (top > bottom)
	{
		int sum = 0;

		for (int number = bottom; number <= top; number++)
		{
			if (number % 2 == 0)
			{
				sum += number;
			}
		}

		return sum;
	}
	else
	{
		return 0;
	}
}

// After
int filterNumber(int number)
{
	if (number % 2 == 0)
	{
		return number;
	}

	return 0;
}

int calculate(int bottom, int top)
{
	if (top < bottom)
	{
		return 0;
	}

	int sum = 0;

	for (int number = bottom; number <= top; number++)
	{
		sum += filterNumber(number);
	}

	return sum;
}
```