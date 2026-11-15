/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <utility>

#include "core/parse.h"
#include "core/coordinate_types.h"
#include "render/display_list.h"
#include "core/resolved_tree.h"
#include "engine/pipeline_guard.h"
#include "engine/update_scheduler.h"
#include "misc/service_registry.h"
#include "loppe_export.h"

// 前向声明：文本测量接口
namespace loppe { class ITextMeasurer; }

// 前向声明
class StyleEngine;
class LayoutEngine;
class BoxModelEngine;
class ASTBuildEngine;

namespace loppe {
	class ResolvedNode;
}

class LOPPE_API DocumentRuntime
{
	protected:
		// 本文档的调度器。
		UpdateScheduler ownedScheduler_;
		ServiceRegistry serviceRegistry;

		template <typename T>
		T* getService() const
		{
			return serviceRegistry.get<T>();
		}

		std::shared_ptr<ASTNode> m_ast;
		void setAST(std::shared_ptr<ASTNode> a);
		// 共享的 ResolvedTree
		loppe::ResolvedTree resolvedTree;
	public:
		DocumentRuntime();
		std::shared_ptr<ASTNode> getAST();

		// 本份文档的调度器：默认就是自有实例；宿主可 setScheduler() 显式改指别的实例。
		UpdateScheduler* scheduler() const;
		void setScheduler(UpdateScheduler* s);

		// 文档生命周期：析构时通知所有订阅者。
		~DocumentRuntime();
		// 销毁通知订阅。
		// 返回订阅 ID，解除时用 unsubscribeDestroy(id)。
		int subscribeDestroy(std::function<void()> cb);
		void unsubscribeDestroy(int id);

		void registerService(AbstractEngine* service);

		template <typename T>
		bool hasService() const
		{
			return serviceRegistry.has<T>();
		}

		// 文本测量器（由 Application 创建并注入）
		loppe::ITextMeasurer* textMeasurer = nullptr;

		// 获取共享的 ResolvedTree
		loppe::ResolvedTree* getResolvedTree();
		const loppe::ResolvedTree* getResolvedTree() const;

		// 文本测量器的注入与访问。
		void setTextMeasurer(loppe::ITextMeasurer* m);
		loppe::ITextMeasurer* getTextMeasurer() const;

		// 便捷访问常用引擎（若已通过 registerService 注册）
		LayoutEngine* getLayoutEngine() const;
		StyleEngine* getStyleEngine() const;
		BoxModelEngine* getBoxModelEngine() const;
		ASTBuildEngine* getASTBuildEngine() const;

		// 初始化/关闭/运行管线接口
		void initializeAll(std::vector<std::type_index> pipelineOrder);
		void shutdownAll();
		// 原始管线：生成 DisplayList
		void runPipeline(loppe::DisplayList& outList, float viewportW, float viewportH);
		void runPipeline(const std::vector<std::type_index>& pipelineOrder, float viewportW, float viewportH);
		// 只解析并填充 ResolvedTree，不生成 DisplayList
		void runPipelineRenderTreeOnly(loppe::ViewportSizeDp viewportSize);
		void runPipelineLayoutOnly(loppe::ViewportSizeDp viewportSize);

		// 管线或更新步骤失败后的恢复入口（内部会记录并安排一次全量重试）。
		void recoverFromPipelineFailure(const char* phase, const std::string& detail);

		// 取走「最近一趟管线里是否有 engine 失败过」的标记（取走即清）；
		// 为真时不要提交这一趟的同步/渲染。
		bool consumePipelineFailure();

		// 通知所有已注册引擎 AST 已更新，并将 AST 更新请求排入 UpdateScheduler
		void notifyASTUpdated(std::shared_ptr<ASTNode> root);

	private:
		UpdateScheduler* scheduler_ = nullptr;
		// 销毁通知订阅者
		std::vector<std::pair<int, std::function<void()>>> destroySubs_;
		int nextSubId_ = 1;
		loppe::PipelineGuard pipelineGuard_{ &resolvedTree };
};