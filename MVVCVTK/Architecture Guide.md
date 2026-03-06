## 分层职责
  - MainWindow: 只做界面状态应用与用户提示
  - TabMap: 只做 Tab 元数据与 Tab->Page 映射
  - WorkspaceFlow: 只做 open/bind/session 业务流程

## 新增 Tab 的标准步骤
  1. 在 TabIndex 增加常量
  2. 在 TabMap::tabNames 增加文案（顺序与 TabIndex 一致）
  3. 在 MainWindow::buildRibbonStack 创建页面并 bindPage
  4. 验证 onTabChanged 无需改业务分支
  5. 跑回归清单

## 禁止回退的规则
  - 禁止在 MainWindow 直接写 openFile + bindSession 细节
  - 禁止新增魔法数字 tab index
  - 禁止在多个地方重复维护同一套 tab 规则