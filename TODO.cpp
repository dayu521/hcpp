// TODO 理解协程取消
// TODO 多线程缓存socket
// TODO 取消dns基于管道的等待
// TODO 功能的设计图
/*TODO二叉树可以保存部分插入模式的信息,而平衡树则基本上不保存这些信息
利用这些信息,可以做缓存,缓存意味着可以随意删除那些不必要的节点
伸展树是一个例子
*/
// TODO dns实现缓存部分,可以直接使用线程局部变量.但其他情况,不应该使用线程局部变量.
//  乍看上去可以尝试通过指针去使用.即取它的地址放到指针中
//   但是,线程终止时,通过指针引用的变量就成为悬挂指针.
// 如果不直接创建线程变量,而创建一个线程变量的指针,它指向某个变量.那这个被指向的变量又不是线程安全的了