# 实验二 语义分析

## 变量类型

下面首先讨论一个变量有哪些类型, 以及类型如何存储并进行比较.

### int, float

int和float是基本类型, 不会再包含额外的信息了. 这里只讨论类型的表示, 不涉及值和变量名的讨论.

它还可以是一些复合类型的基类型, 比如struct的field, 以及一维的array.

### array

array需要记录自己的大小, 或者说是元素个数. 多维array是通过type域链接array进行体现的.

在最后一维里, array的type域可以链接到基类型, struct, 以及函数类型.

### struct, field

struct类型一部分由名字体现, 一部分由field的布局体现. 
所以field的结构就比较特殊了, 它不仅要链接自己的类型, 还需要将链接自己的兄弟.

function

函数的属性有返回类型以及参数个数, 顺序, 类型. 可以看做是加了返回类型的struct.

## Errors

```c
a[1.5 + 2]
```

how many errors should it report?

```c
struct A { int a; int b; };
A a;
a.a?
```

will this cause name collision?

### 一些存疑的语义错误

```c
struct A { int x; };
struct B { int y; };
int foo() {
    int a[1][2];
    struct a b;

    A = B;
}
```

由于我们的任务是允许结构等价, 所以`struct A`和`struct B`的变量进行运算是允许的.
但是问题是`A = B`该如何界定, 是认为两边都是没有定义的变量呢, 还是说不是合法的操作数呢, 这种情况下要不要判断左值呢?
还有就是`struct a b`, 这种情况是认为没有定义这个类型呢, 还是说类型不合法?