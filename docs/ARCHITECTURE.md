# Penda TSF 0.2 架构

## 系统输入层

`src/Penda.Tsf` 是 C++20 原生 COM DLL，分别构建 x64 / Win32，静态链接 C++ Runtime。ITfTextInputProcessorEx 激活服务，ITfKeyEventSink 接收 TSF 路由按键，ITfEditSession 申请上下文读写锁，ITfContextComposition / ITfComposition 管理应用内组合范围。

输入字符更新组合范围和选择，选词通过 range.SetText + EndComposition 完成。没有全局键盘钩子、模拟按键、剪贴板粘贴或跨进程 CLR 查询。异步编辑会话持有服务与上下文引用，使用焦点代次拒绝过期操作。失败的同步写锁可降级为异步请求。

组合终止回调释放状态；线程焦点、文档焦点、上下文栈变化结束旧组合；文本编辑通知检测光标是否离开组合范围。焦点变化结束组合时保留应用中已有原始编码，不改动用户的新选择。布局通知在读会话中更新候选位置。候选鼠标点击通过异步写会话提交。

按键测试只查询状态，不修改组合或选择。TSF 上下文状态、禁用/空上下文 compartment，以及 GUID_PROP_INPUTSCOPE 中的 Password / Private 决定是否透传。应用未正确报告输入作用域的兼容性仍需实测。

## 候选窗

原生 GDI 双缓冲窗口，WS_EX_NOACTIVATE、WS_EX_TOOLWINDOW、置顶；六行分页、语言标记、DPI 缩放、屏幕工作区边界约束。光标来自 TSF 文本范围，而非鼠标位置。退出服务销毁窗口并尝试注销窗口类，防止 DLL 卸载后留下无效窗口过程。

目前是桌面候选 UI，尚未实现 ITfUIElement / ITfCandidateListUIElement、UI-less 宿主、显示属性提供器、语言栏自定义按钮或系统开关 compartment 联动。只注册实际支持的 GUID_TFCAT_TIP_KEYBOARD，不冒称 secure/immersive/comless 能力。系统输入法选择器通过已注册语言配置显示 Penda。

## 设置与词库

`Penda.Core` 保留词库导入/导出、SCEL、罗马字与宽度逻辑。`Penda.Windows` 只作为设置和预览程序；SettingsService 不接收系统按键。NativeSnapshot 将三语独立词库、语言开关、宽度与罗马字映射编译为版本化 UTF-16LE `engine.pdat`，同目录临时文件替换提交。

原生解析器有文件大小、词条数、字符串长度、排序与枚举值校验；损坏的新索引不替换已有快照。发布包带默认索引，用户快照位于 LocalAppData\Penda。每进程共享缓存，首次激活读取；新组合仅检查文件时间戳。检测更新后在线程池后台解析，完成后切换快照；现有组合保留自己的 shared_ptr，不被后台更新破坏。后台回调持有 DLL 模块引用，返回时释放，避免卸载竞态。

原生候选查询使用排序数组和二分定位，每语最多扫描 192 条，再按语言交错返回最多 36 条。单个进程内服务实例共享索引；不同应用进程仍各自拥有字符串索引。大型词库的首次加载耗时、跨进程总内存仍需优化，后续可改为内存映射紧凑 FST。

Ctrl+Alt+1/2/3 是当前服务实例的临时语言掩码，不会写磁盘。保存的新设置加载后更新默认掩码。Ctrl+Alt+空格是实例内直通开关，不等同于 Windows 的全局输入模式 compartment。

## 注册与安装

COM CLSID：{245A7FC9-923C-4C9C-A863-8602A83C3752}。
TSF Profile：{D15542B2-92E7-48EE-B5CA-38933AA62E9C}，LANGID 0x0804。

DllGetClassObject / DllCanUnloadNow 实现 COM 工厂与对象计数。DllRegisterServer 写对应架构的机器 COM InprocServer32（Apartment），注册 TSF 输入处理器、语言配置与键盘类别。DllUnregisterServer 取消上述注册。只操作本组件的明确注册表键；脚本不删除程序或用户数据文件。

双架构注册必须分别用 System32 与 SysWOW64 regsvr32。安装包采用管理员安装到 Program Files。TSF 由 Windows 加载，无需设置托盘自启动。注册/安装尚未在此开发会话执行，需要登录后真实应用验收。安装包需要本机额外安装 Inno Setup 6 后编译。

## 检查与限制

`test-tsf.ps1` 运行原有 25 项托管检查，生成四种设置/88 个输入的一致性数据，并分别运行 x64/x86 原生测试。测试使用真正的 TSF ThreadMgr、DocumentMgr、Context、内存 ITextStoreACP 及编辑锁，只对键盘接收器安装与按键分发做测试替代，以避免修改系统注册。覆盖组合文字、提交/取消/退格/标点/翻页/数字选择、只读/禁用上下文以及 COM 卸载计数。

该验证不覆盖系统已安装 profile 的真实激活、物理键盘、用户真实 SCEL、UWP、密码作用域宿主、远程桌面或安装升级。当前语言能力仍是词库匹配和基础罗马字转换；未加入整句解码、用户学习或日语文节活用。ThemeId 保留，原生 UI 与引擎分离，但自定义主题加载尚未实现。

参考：
- https://learn.microsoft.com/en-us/windows/win32/tsf/text-service-registration
- https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfkeystrokemgr-advisekeyeventsink
- https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfeditsession

## 0.9.0 兼容性补充（覆盖上文 0.2 的旧限制描述）
TextService 持有 CandidateUi，并通过真实 ITfUIElementMgr 注册、更新、结束候选 UI。宿主拒绝显示自绘窗口时仍暴露完整活动语言候选，支持 ITfCandidateListUIElementBehavior.SetSelection / Finalize / Abort；候选文本快照与回调生命周期分离，结束时 Detach，避免宿主持有过期服务指针。默认每页十项，宿主可以传入自己的页边界。
NativeSnapshot 更新用户索引后发布只读 Runtime 缓存。Engine 使用未重定向的用户路径，并在读取权限不足时选择缓存；安装包备用快照也保持当前完整词库。新代码不能直接解决所有 Win32 原始输入/IMM32 游戏，因此测试结论只覆盖标准 TSF 和 UI-less 合同。不要通过注入游戏、模拟输入或关闭反作弊来绕过限制。
兼容测试建立了 Penda.ReadOnlyDictionary.Probe 专用、无网络能力的 Windows 应用容器测试配置，保留供重复验证；测试项目和代码仍在 E:/projects/penda。未删除任何配置目录。测试工具可使用 --container-check 验证实际令牌和完整词库读取。
