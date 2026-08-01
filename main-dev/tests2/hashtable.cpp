#include<iostream>
#include<cstring>
using namespace std;

const int N = 100003;
int Hash[N], e[N], ne[N], idx;

void insert(int x){
    int k = (x % N + N) % N;  //余数可能为负数
    e[idx] = x;
    ne[idx] = Hash[k];
    Hash[k] = idx++;
}

bool find(int x){
    int k = (x % N + N) % N;
    for(int i = Hash[k]; i != -1; i = ne[i])
        if(e[i] == x)
            return true;
    return false;
}

int main(){
    cin.tie(0); cout.tie(0);
    int n;  cin>>n;

    memset(Hash, -1, sizeof(Hash));

    while(n--){
        char op;
        int x;
        cin>>op>>x;

        if(op == 'I')
            insert(x);
        else{
            if(find(x))
                puts("Yes");
            else
                puts("No");
        }
    }
}
