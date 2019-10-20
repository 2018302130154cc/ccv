#include "ccv_nnc.h"
#include "ccv_nnc_easy.h"
#include "ccv_nnc_internal.h"
#include "ccv_nnc_easy.h"
#include "ccv_internal.h"
#include "_ccv_nnc_dynamic_graph.h"
#include "_ccv_cnnp_model.h"

#pragma mark - Level-5.5 API

static int _ccv_cnnp_model_exec(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* const inputs, const int input_size, ccv_nnc_tensor_t* const* const outputs, const int output_size, ccv_nnc_stream_context_t* const stream_context)
{
	ccv_cnnp_model_t* const model = (ccv_cnnp_model_t*)cmd.data;
	if (cmd.cmd == CCV_NNC_CUSTOM_FORWARD)
	{
		ccv_cnnp_model_evaluate(model, (ccv_cnnp_evaluate_param_t){
			.requires_grad = 1,
			.disable_outgrad = 0,
			.is_test = 0,
		}, inputs, input_size, outputs, output_size, 0, stream_context);
	} else {
		ccv_cnnp_compiled_data_t* const compiled_data = model->compiled_data;
		const int parallel_count = ccv_max(compiled_data->parallel_count, 1);
		const int ingrad_size = model->output_size * parallel_count;
		assert(ingrad_size <= input_size);
		ccv_cnnp_model_backward(model, inputs, ingrad_size, outputs, output_size, 0, stream_context);
	}
	return CCV_NNC_EXEC_SUCCESS;
}

static void _ccv_cnnp_model_apply_gradients(const ccv_nnc_cmd_t cmd, const ccv_nnc_cmd_t minimizer, ccv_nnc_stream_context_t* const stream_context)
{
	ccv_cnnp_model_t* const model = (ccv_cnnp_model_t*)cmd.data;
	ccv_cnnp_model_set_minimizer(model, minimizer, 0, 0);
	ccv_cnnp_model_apply_gradients(model, stream_context);
}

static ccv_nnc_stateful_cmd_vtab_t ccv_cnnp_model_exec_isa = {
	.super = {
		.exec = _ccv_cnnp_model_exec,
	},
	.apply_gradients = _ccv_cnnp_model_apply_gradients,
};

void ccv_nnc_dynamic_graph_evaluate(ccv_nnc_dynamic_graph_t* const dynamic_graph, ccv_cnnp_model_t* const model, const ccv_nnc_tensor_variable_t* const inputs, const int input_size, ccv_nnc_tensor_variable_t* const outputs, const int output_size, ccv_nnc_tensor_tape_t* const tensor_tape)
{
	ccv_nnc_cmd_t cmd = ccv_nnc_cmd(CCV_NNC_CUSTOM_FORWARD, (ccv_nnc_cmd_vtab_t*)&ccv_cnnp_model_exec_isa, (ccv_nnc_cmd_param_t){}, 0);
	cmd.data = model;
	assert(input_size > 0);
	if (!model->graph)
	{
		ccv_nnc_tensor_param_t input_params[input_size];
		int i;
		for (i = 0; i < input_size; i++)
			input_params[i] = inputs[i]->info;
		// TODO: when I switch minimizer, the saved_aux will be wrong. Need to reconstruct the graph, re-allocate saved_aux to correct this.
		ccv_cnnp_model_compile(model, input_params, input_size, CMD_SGD_FORWARD(0, 0, 0, 0, 0, 0), CMD_NOOP());
	}
	const ccv_nnc_graph_exec_symbol_t symbol = ccv_nnc_dynamic_graph_exec_ret(dynamic_graph, cmd, ccv_nnc_no_hint, 0, inputs, input_size, outputs, output_size);
	int ret;
	kh_put(stateful_exec, dynamic_graph->stateful_execs, symbol.d, &ret);
}
