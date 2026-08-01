#include<iostream>
#include<vector>
#include<time.h>
using namespace std;

/*                               1                                  */
//左右指针法
int Partition_Hoare(vector<int> &a, int left, int right){
	int i = left;
	int j = right;
	int key = a[left];

	while(i != j){
	    while(i < j && a[j] >= key)      	 //向左找到小于基准值的值的下标
                j--;
            while(i < j && a[i] <= key)      	 //向右找到大于基准值的值的下标
                i++;
	    swap(a[i], a[j]);
	}
	/*   i等于j时跳出循环 当前基准值此时在下标为i的位置(合适的位置)   */
	swap(a[left], a[i]);	                 //最左边的元素变为处于当前合适位置的元素,把基准值放在合适位置
	return i;                                //返回合适位置(i,j都可以)
}



/*                                 2                                */
//挖坑法1
int Partition_DigI(vector<int> &a, int left, int right){
    int i = left;
    int j = right;
    int key = a[left];

    while(i != j){
	while(i < j && a[j] >= key)          //必须先动右边的 因为取的基准值在左边
	    j--;                             //向左寻找直到找到比基准值小的
	a[i] = a[j];                         //挖坑 填入比基准值小的值

	while(i < j && a[i] <= key)          //再动左边的
	    i++;                             //向右寻找直到找到比基准值大的
	a[j] = a[i];                         //挖坑 填入比基准值大的值
    }
    /*   i等于j时跳出循环 下标为i的位置即为合适的插入位置   */
    a[i] = key;                              //在i,j相遇的地方填入基准值
    return i;
}

//挖坑法2
int Partition_DigII(vector<int> &a, int left, int right){
    int i = left;                            //从左边开始
    int j = right;                           //从右边开始
    int key = a[left];                       //将最左边的数设为基准值

    while(i != j){
	while(i < j && a[j] >= key)          //必须先动右边的 因为取的基准值在左边
	    j--;                             //向左寻找直到找到比基准值小的
	swap(a[i], a[j]);                    //交换
	i++;                                 //看下一个（可以省略）

	while(i < j && a[i] <= key)          //再动左边的
	    i++;                             //向右寻找直到找到比基准值大的
	swap(a[i], a[j]);                    //交换
	j--;                                 //看下一个（可以省略）
	}
	/* 一轮循环后基准值所在位置左边的数都比基准值小,右边的都比基准值大 */
    /*   i等于j时跳出循环 当前基准值此时在下标为i的位置(合适的位置)   */
    return i;                                //返回基准值合适位置
}



/*                               3                                  */
//前后指针法
int Partition_PreAndCurr(vector<int> &a, int left, int right){
    int pre = left;                          //pre最后找到左半部分最后一个小于基准值的元素
    int curr = pre + 1;                      //curr找到pre指向元素之后的下一个小于基准值的元素
    int key = a[left];

    while(curr <= right){
	if(a[curr] < key && ++pre != curr)   //当a[curr]>=key时pre保持不动
	    swap(a[curr], a[pre]);           //pre往后移动到其下一个位置,并交换前后指针指向的元素
	curr++;
    }
    swap(a[left], a[pre]);                   //最后将基准值和此时处于pre位置的小于基准值的元素交换
    return pre;
}



/*                               4                                  */
//取基准值的优化    三数取中
int GetMid(vector<int> &a, int left, int right){
	int mid = (left + right)>>1;
	if(a[left] < a[mid])
	    if(a[mid] < a[right])
		return mid;                      //mid为中间值
	    else
		if(a[left] < a[right])
		    return right;                //right为中间值
		else
		    return left;                 //left为中间值
	else
	    if(a[mid] > a[right])
		return mid;                      //mid为中间值
	    else
		if(a[left] > a[right])
		    return right;                //right为中间值
		else
		    return left;                 //left为中间值
}


//左右指针法 三数取中优化
int Partition_Better(vector<int> &a, int left, int right){
    int pos = GetMid(a, left, right);
    swap(a[pos], a[left]);                   //将取得的基准值和第一个位置交换

    int i = left;
    int j = right;
    int key = a[left];

    while(i != j){
	while(i < j && a[j] >= key)
	    j--;
	while(i < j && a[i] <= key)
	    i++;
	swap(a[i], a[j]);
    }

    swap(a[i], a[left]);
    return i;
}



/*******************************************************************/
void Quick_Sort(vector<int> &a, int left, int right){
    if( left >= right )
	return;

    //int i = Partition_Hoare(a, left ,right); //分割    C(n) = n - 1
    //int i = Partition_DigI(a, left, right);
    //int i = Partition_DigII(a, left, right);
    int i = Partition_PreAndCurr(a, left, right);

    //int i = Partition_Better(a, left, right);
    Quick_Sort(a, left, i - 1);              //递归左边的部分
    Quick_Sort(a, i + 1, right);             //递归右边的部分
}


void show(vector<int> &v){
    for(auto &x : v)
	cout<<x<<" ";
    cout<<endl;
}


main(){
    vector<int> v;
    srand((int)time(0));
    int n = 50;
    while(n--)
	v.push_back(rand() % 100 + 1);
    show(v);

    Quick_Sort(v, 0, v.size() - 1);

    cout<<endl<<endl;
    show(v);

    /* 作者 Amαdeus */
}
