#ifndef _error_h_
#define _error_h_

#define ERROR_OK                    0       // 正确执行
#define ERROR_PENDING				1		// 后台执行中
//add by tz 2012-11-23 for send signal to user to clear screen
#define ERROR_OK_LIVE_END           10       // 实时码流线程关闭
#define ERROR_OK_DOWNLOAD_RECORD    11       // 录像文件下载完毕
//add by tz 2012-11-23 for send signal to user to clear screen

#define ERROR_UNKOWN				-1		// 未知错误
#define ERROR_MEMORY				-26		// 分配内存失败
#define ERROR_NO_PRIVILEGE			-88		// 权限错误
#define ERROR_USER_OR_PASSWORD		-89		// 用户名或密码错误
#define ERROR_NO_OBJECT             -241    // ID无效
#define ERROR_OBJECT_EXISTS         -242    // 用户已注册
#define ERROR_STREAM_MISMATCH	    -254  //POSA流格式不匹配(无法识别流格式)
#define ERROR_PARAM					-501	// 非法参数
#define ERROR_CONNECT				-502	// 连接服务器失败
#define ERROR_NOT_CONNECT			-506	// 未连接服务器
#define ERROR_RECV					-507	// 接收失败
#define ERROR_SEND					-508	// 发送失败
#define ERROR_REPLY					-512	// 接收到错误的应答
#define ERROR_NOT_LOGIN				-522	// 用户没有登录
#define ERROR_RECV_TIMEOUT			-523	// 网络接收超时
#define ERROR_SEND_TIMEOUT			-599	// 网络发送超时
#define ERROR_HTTP_REDIRECT			-598	// HTTP重定向
#define ERROR_NOTFOUND				-597	// XXX不存在
#define ERROR_NOT_EXIST				-596	// XXX不存在
#define ERROR_NOT_IMPLEMENT			-595	// 功能未实现
#define ERROR_INVALID_INSTRUCTION	-594	// 非法指令
#define ERROR_OS_RESOURCE			-593	// 系统资源错误
#define ERROR_EXIST					-592	// XXX已存在


#endif /* !_error_h_ */
