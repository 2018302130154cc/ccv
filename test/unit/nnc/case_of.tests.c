#include "case.h"
#include "ccv_case.h"
#include "ccv_nnc_case.h"
#include <ccv.h>
#include <nnc/ccv_nnc.h>
#include <nnc/ccv_nnc_easy.h>
#include <3rdparty/dsfmt/dSFMT.h>

TEST_SETUP()
{
	ccv_nnc_init();
}

static int piecewise_case_of(ccv_nnc_tensor_t* const* const inputs, const int input_size, const void* const data)
{
	if (inputs[0]->data.f32[0] < 0)
		return 0;
	else if (inputs[0]->data.f32[0] < 1)
		return -1; // Pass through because the computation is essentially x * 1
	else if (inputs[0]->data.f32[0] < 2)
		return 1;
	else
		return 2;
}

TEST_CASE("graph for a piece-wise linear function")
{
	ccv_nnc_graph_t* const graph = ccv_nnc_graph_new();
	ccv_nnc_tensor_t* x = ccv_nnc_tensor_new(0, ONE_CPU_TENSOR(1), 0);
	ccv_nnc_graph_exec_t case_of = ccv_nnc_graph_case_of_new(graph, CCV_NNC_GRAPH_FORWARD, TENSOR_LIST(x));
	ccv_nnc_graph_set_case_of_expr(graph, case_of, piecewise_case_of, 0);
	ccv_nnc_graph_t* graph_0 = ccv_nnc_graph_new();
	ccv_nnc_graph_exec_t set_0 = ccv_nnc_graph_exec_new(graph_0, ccv_nnc_cmd(CCV_NNC_SET_FORWARD, 0, CMD_GENERIC(), 0), ccv_nnc_no_hint, 0, 0, TENSOR_LIST(x));
	ccv_nnc_graph_set_sources(graph_0, GRAPH_EXEC_LIST(set_0));
	ccv_nnc_graph_set_destinations(graph_0, GRAPH_EXEC_LIST(set_0));
	ccv_nnc_graph_set_case_of(graph, case_of, graph_0, 0);
	ccv_nnc_graph_t* graph_1 = ccv_nnc_graph_new();
	ccv_nnc_tensor_t* p1 = ccv_nnc_tensor_new(0, ONE_CPU_TENSOR(1), 0);
	p1->data.f32[0] = 0.5;
	ccv_nnc_tensor_t* s1 = ccv_nnc_tensor_new(0, ONE_CPU_TENSOR(1), 0);
	s1->data.f32[0] = 0.5;
	ccv_nnc_graph_exec_t prod_1 = ccv_nnc_graph_exec_new(graph_1, ccv_nnc_cmd(CCV_NNC_EWPROD_FORWARD, 0, CMD_GENERIC(), 0), ccv_nnc_no_hint, TENSOR_LIST(x, p1), TENSOR_LIST(x));
	ccv_nnc_graph_exec_t sum_1 = ccv_nnc_graph_exec_new(graph_1, ccv_nnc_cmd(CCV_NNC_EWSUM_FORWARD, 0, CMD_GENERIC(), 0), ccv_nnc_no_hint, TENSOR_LIST(x, s1), TENSOR_LIST(x));
	ccv_nnc_graph_set_sources(graph_1, GRAPH_EXEC_LIST(prod_1));
	ccv_nnc_graph_set_destinations(graph_1, GRAPH_EXEC_LIST(sum_1));
	ccv_nnc_graph_set_case_of(graph, case_of, graph_1, 1);
	ccv_nnc_graph_t* graph_2 = ccv_nnc_graph_new();
	ccv_nnc_graph_exec_t set_2 = ccv_nnc_graph_exec_new(graph_2, ccv_nnc_cmd(CCV_NNC_SET_FORWARD, 0, CMD_BLAS(1.5), 0), ccv_nnc_no_hint, 0, 0, TENSOR_LIST(x));
	ccv_nnc_graph_set_sources(graph_2, GRAPH_EXEC_LIST(set_2));
	ccv_nnc_graph_set_destinations(graph_2, GRAPH_EXEC_LIST(set_2));
	ccv_nnc_graph_set_case_of(graph, case_of, graph_2, 2);
	x->data.f32[0] = -1;
	ccv_nnc_graph_run(graph, 0, 0, GRAPH_EXEC_LIST(case_of), GRAPH_EXEC_LIST(case_of));
	REQUIRE_EQ_WITH_TOLERANCE(x->data.f32[0], 0, 1e-5, "in negative region should equal to 0");
	x->data.f32[0] = 0.76;
	ccv_nnc_graph_run(graph, 0, 0, GRAPH_EXEC_LIST(case_of), GRAPH_EXEC_LIST(case_of));
	REQUIRE_EQ_WITH_TOLERANCE(x->data.f32[0], 0.76, 1e-5, "y = x in (0, 1)");
	x->data.f32[0] = 1.226;
	ccv_nnc_graph_run(graph, 0, 0, GRAPH_EXEC_LIST(case_of), GRAPH_EXEC_LIST(case_of));
	REQUIRE_EQ_WITH_TOLERANCE(x->data.f32[0], (1.226 - 1) * 0.5 + 1, 1e-5, "y = (x - 1) * 0.5 + 1 in (1, 2)");
	x->data.f32[0] = 2.1;
	ccv_nnc_graph_run(graph, 0, 0, GRAPH_EXEC_LIST(case_of), GRAPH_EXEC_LIST(case_of));
	REQUIRE_EQ_WITH_TOLERANCE(x->data.f32[0], 1.5, 1e-5, "y = 1.5 if x > 2");
	ccv_nnc_tensor_free(x);
	ccv_nnc_tensor_free(p1);
	ccv_nnc_tensor_free(s1);
	ccv_nnc_graph_free(graph);
}

#include "case_main.h"