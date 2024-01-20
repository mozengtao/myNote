- 冒泡排序
	- 相邻元素之间进行比较，如果条件不满足则交换顺序。为了保证有序性，排序需要进行多次，每次排序都会将未排序的最大元素放到序列的末尾
	- 时间复杂度为O(n^2)
	- 空间复杂度为O(1)
	- c语言实现
	  collapsed:: true
		- ```c
		  void bubbleSort(int arr[], int n)
		  {
		    int i, j, temp;
		    for(i = 0; i < n - 1; i++) { // 外层循环控制排序趟数
		      for(j = 0; j < n - i - 1; j++) { // 内层循环控制单趟比较次数
		        if(arr[j] > arr[j+1]) {
		          temp = arr[j];
		          arr[j] = arr[j+1];
		          arr[j+1] = temp;
		        }
		      }
		    }
		  }
		  ```
- 插入排序
- 选择排序
- 快速排序
- 归并排序
- 堆排序