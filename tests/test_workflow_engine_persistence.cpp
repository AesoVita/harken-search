#include "WorkflowEngine.h"

#include <filesystem>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "aris_stage6_workflow_pal_test";
    const fs::path file = root / "workflows.txt";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    auto& workflows = WorkflowEngine::getWorkflows();
    workflows.clear();

    Workflow workflow;
    workflow.name = "PAL Roundtrip";
    workflow.description = "preserve workflow text";
    WorkflowNode node;
    node.type = WorkflowNodeType::SetField;
    node.field = "Description";
    node.value = "Footsteps";
    workflow.nodes.push_back(node);
    WorkflowEngine::addWorkflow(workflow);

    if (!WorkflowEngine::saveWorkflowsToFile(file.u8string())) {
        std::cerr << "FAIL: workflow save should succeed\n";
        return 1;
    }

    workflows.clear();
    if (!WorkflowEngine::loadWorkflowsFromFile(file.u8string()) || workflows.size() != 1 ||
        workflows[0].name != workflow.name || workflows[0].description != workflow.description ||
        workflows[0].nodes.size() != 1 || workflows[0].nodes[0].value != node.value) {
        std::cerr << "FAIL: workflow persistence should roundtrip through the PAL\n";
        fs::remove_all(root, ec);
        return 1;
    }
    if (WorkflowEngine::loadWorkflowsFromFile((root / "missing.txt").u8string())) {
        std::cerr << "FAIL: missing workflow file should fail to load\n";
        fs::remove_all(root, ec);
        return 1;
    }

    workflows.clear();
    fs::remove_all(root, ec);
    std::cout << "PASS: workflow persistence preserves PAL roundtrip semantics\n";
    return 0;
}
