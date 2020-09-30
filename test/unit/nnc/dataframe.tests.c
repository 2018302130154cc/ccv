#include "case.h"
#include "ccv_case.h"
#include "ccv_nnc_case.h"
#include <ccv.h>
#include <nnc/ccv_nnc.h>
#include <nnc/ccv_nnc_easy.h>
#include "3rdparty/dsfmt/dSFMT.h"

TEST_SETUP()
{
	ccv_nnc_init();
}

static int _ccv_iter_accessed = 0;

static void _ccv_iter_int(const int column_idx, const int* row_idxs, const int row_size, void** const data, void* const context, ccv_nnc_stream_context_t* const stream_context)
{
	int* const array = (int*)context;
	int i;
	for (i = 0; i < row_size; i++)
		data[i] = (void*)(intptr_t)array[row_idxs[i]];
	++_ccv_iter_accessed;
}

TEST_CASE("iterate through a simple dataframe")
{
	int int_array[8] = {
		0, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	ccv_cnnp_dataframe_iter_t* const iter1 = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(0));
	int result[8];
	int i = 0;
	void* data;
	while (0 == ccv_cnnp_dataframe_iter_next(iter1, &data, 1, 0))
		result[i++] = (int)(intptr_t)data;
	ccv_cnnp_dataframe_iter_free(iter1);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	// iter2 test some prefetch capacities.
	ccv_cnnp_dataframe_iter_t* const iter2 = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(0));
	for (i = 0; i < 3; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	_ccv_iter_accessed = 0;
	for (i = 0; i < 3; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "the iterator is not accessed at all, because prefetching");
	ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
	result[3] = (int)(intptr_t)data;
	REQUIRE_EQ(_ccv_iter_accessed, 1, "the iterator is accessed, because no prefetching");
	ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	REQUIRE_EQ(_ccv_iter_accessed, 2, "the iterator is accessed again, for prefetching");
	_ccv_iter_accessed = 0;
	for (i = 4; i < 8; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 3, "the iterator accessed 3 times, the first is prefetching");
	ccv_cnnp_dataframe_iter_free(iter2);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	// iter3 test more prefetch behavior.
	ccv_cnnp_dataframe_iter_t* const iter3 = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(0));
	for (i = 0; i < 3; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
	for (i = 0; i < 2; i++)
	{
		_ccv_iter_accessed = 0;
		ccv_cnnp_dataframe_iter_next(iter3, &data, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
		result[i] = (int)(intptr_t)data;
		ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 1, "iterator accessed for prefetching");
	}
	for (i = 2; i < 4; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
	_ccv_iter_accessed = 0;
	for (i = 2; i < 7; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter3, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
	ccv_cnnp_dataframe_iter_next(iter3, &data, 1, 0);
	result[7] = (int)(intptr_t)data;
	REQUIRE_EQ(_ccv_iter_accessed, 1, "iterator accessed twice");
	const int fail1 = ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
	REQUIRE_EQ(fail1, -1, "cannot advance no more");
	ccv_cnnp_dataframe_iter_free(iter3);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	ccv_cnnp_dataframe_free(dataframe);
}

static void _ccv_int_plus_1(void* const* const* const column_data, const int column_size, const int batch_size, void** const data, void* const context, ccv_nnc_stream_context_t* const stream_context)
{
	int i;
	for (i = 0; i < batch_size; i++)
	{
		int k = (int)(intptr_t)column_data[0][i];
		data[i] = (void*)(intptr_t)(k + 1);
	}
}

TEST_CASE("iterate through derived column")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	int result[8];
	int i = 0;
	void* data;
	while (0 == ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
		result[i++] = (int)(intptr_t)data;
	ccv_cnnp_dataframe_iter_free(iter);
	for (i = 0; i < 8; i++)
		++int_array[i];
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	// iter2 test some prefetch capacities.
	ccv_cnnp_dataframe_iter_t* const iter2 = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	for (i = 0; i < 3; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	_ccv_iter_accessed = 0;
	for (i = 0; i < 3; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "the iterator is not accessed at all, because prefetching");
	ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
	result[3] = (int)(intptr_t)data;
	REQUIRE_EQ(_ccv_iter_accessed, 1, "the iterator is accessed, because no prefetching");
	ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	REQUIRE_EQ(_ccv_iter_accessed, 2, "the iterator is accessed again, for prefetching");
	_ccv_iter_accessed = 0;
	for (i = 4; i < 6; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 1, "the iterator accessed 3 times, the first is prefetching");
	const int success0 = ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	REQUIRE_EQ(success0, 0, "success");
	const int success1 = ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	REQUIRE_EQ(success1, 0, "success");
	const int fail0 = ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	REQUIRE_EQ(fail0, -1, "should fail");
	for (i = 0; i < 6; i++)
		++int_array[i];
	ccv_cnnp_dataframe_iter_free(iter2);
	REQUIRE_ARRAY_EQ(int, int_array, result, 6, "iterated result and actual result should be the same up to 6");
	// iter3 test more prefetch behavior.
	ccv_cnnp_dataframe_iter_t* const iter3 = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	for (i = 0; i < 3; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
	for (i = 0; i < 2; i++)
	{
		_ccv_iter_accessed = 0;
		ccv_cnnp_dataframe_iter_next(iter3, &data, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
		result[i] = (int)(intptr_t)data;
		ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 1, "iterator accessed for prefetching");
	}
	for (i = 2; i < 4; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
	_ccv_iter_accessed = 0;
	for (i = 2; i < 7; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter3, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
	ccv_cnnp_dataframe_iter_next(iter3, &data, 1, 0);
	result[7] = (int)(intptr_t)data;
	REQUIRE_EQ(_ccv_iter_accessed, 1, "iterator accessed twice");
	const int fail1 = ccv_cnnp_dataframe_iter_prefetch(iter3, 1, 0);
	REQUIRE_EQ(fail1, -1, "cannot advance no more");
	for (i = 0; i < 8; i++)
		++int_array[i];
	ccv_cnnp_dataframe_iter_free(iter3);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("iterate through derived column with cursor reset")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	int i;
	void* data;
	int result[8];
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	for (i = 0; i < 3; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter, 1, 0);
	for (i = 0; i < 2; i++)
	{
		_ccv_iter_accessed = 0;
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
		result[i] = (int)(intptr_t)data;
		ccv_cnnp_dataframe_iter_prefetch(iter, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 1, "iterator accessed for prefetching");
	}
	for (i = 2; i < 4; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter, 1, 0);
	ccv_cnnp_dataframe_iter_set_cursor(iter, 0);
	ccv_cnnp_dataframe_iter_prefetch(iter, 1, 0);
	ccv_cnnp_dataframe_iter_prefetch(iter, 1, 0);
	_ccv_iter_accessed = 0;
	for (i = 0; i < 7; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 5, "5 iterator accessed");
	_ccv_iter_accessed = 0;
	ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
	result[7] = (int)(intptr_t)data;
	REQUIRE_EQ(_ccv_iter_accessed, 1, "iterator accessed");
	const int fail1 = ccv_cnnp_dataframe_iter_prefetch(iter, 1, 0);
	REQUIRE_EQ(fail1, -1, "cannot advance no more");
	for (i = 0; i < 8; i++)
		++int_array[i];
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("iterate with prefetch more than 1 item, variant 1")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	int i;
	void* data;
	int result[8];
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	_ccv_iter_accessed = 0;
	ccv_cnnp_dataframe_iter_prefetch(iter, 3, 0);
	REQUIRE_EQ(_ccv_iter_accessed, 1, "iter accessed once for prefetching");
	for (i = 0; i < 2; i++)
	{
		_ccv_iter_accessed = 0;
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
		result[i] = (int)(intptr_t)data;
		ccv_cnnp_dataframe_iter_prefetch(iter, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 1, "iterator accessed for prefetching");
	}
	ccv_cnnp_dataframe_iter_prefetch(iter, 4, 0);
	_ccv_iter_accessed = 0;
	for (i = 2; i < 8; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
	for (i = 0; i < 8; i++)
		++int_array[i];
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("iterate with prefetch more than 1 item, variant 2")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	int i;
	void* data;
	int result[8];
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	_ccv_iter_accessed = 0;
	ccv_cnnp_dataframe_iter_prefetch(iter, 3, 0);
	REQUIRE_EQ(_ccv_iter_accessed, 1, "iter accessed once for prefetching");
	for (i = 0; i < 3; i++)
	{
		_ccv_iter_accessed = 0;
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
		result[i] = (int)(intptr_t)data;
	}
	ccv_cnnp_dataframe_iter_prefetch(iter, 2, 0);
	ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
	result[3] = (int)(intptr_t)data;
	// Now, I have 1 prefetched, in the middle, and can prefetch 2 more without reallocating.
	ccv_cnnp_dataframe_iter_prefetch(iter, 3, 0);
	_ccv_iter_accessed = 0;
	for (i = 4; i < 8; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
	for (i = 0; i < 8; i++)
		++int_array[i];
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("data is shuffled")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	ccv_cnnp_dataframe_shuffle(dataframe);
	int i;
	void* data;
	int result[8];
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	_ccv_iter_accessed = 0;
	ccv_cnnp_dataframe_iter_prefetch(iter, 3, 0);
	REQUIRE_EQ(_ccv_iter_accessed, 1, "iter accessed once for prefetching");
	for (i = 0; i < 3; i++)
	{
		_ccv_iter_accessed = 0;
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		REQUIRE_EQ(_ccv_iter_accessed, 0, "no iterator accessed");
		result[i] = (int)(intptr_t)data;
	}
	ccv_cnnp_dataframe_iter_prefetch(iter, 2, 0);
	ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
	result[3] = (int)(intptr_t)data;
	ccv_cnnp_dataframe_iter_prefetch(iter, 2, 0);
	_ccv_iter_accessed = 0;
	for (i = 4; i < 8; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 1, "no iterator accessed");
	for (i = 0; i < 8; i++)
		++int_array[i];
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_ARRAY_NOT_EQ(int, int_array, result, 8, "iterated result and actual result should not be the same");
	int covered[8] = {};
	for (i = 0; i < 8; i++)
		covered[result[i] - 3] = 1;
	int is_covered[8] = {
		1, 1, 1, 1, 1, 1, 1, 1
	};
	REQUIRE_ARRAY_EQ(int, covered, is_covered, 8, "data should covered all");
	ccv_cnnp_dataframe_free(dataframe);
}

static void _ccv_iter_reduce_int(void* const* const input_data, const int batch_size, void** const data, void* const context, ccv_nnc_stream_context_t* const stream_context)
{
	int i;
	int total = 0;
	for (i = 0; i < batch_size; i++)
		total += (int)(intptr_t)input_data[i];
	data[0] = (void*)(intptr_t)total;
}

TEST_CASE("dataframe reduce")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	ccv_cnnp_dataframe_t* const reduce = ccv_cnnp_dataframe_reduce_new(dataframe, _ccv_iter_reduce_int, 0, 0, 3, 0, 0);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(reduce, COLUMN_ID_LIST(0));
	int i;
	void* data;
	int result[3];
	_ccv_iter_accessed = 0;
	for (i = 0; ; i++)
	{
		if (0 != ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
			break;
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 3, "should only accessed iterator 3 times");
	int should_result[3] = {
		9, 18, 17
	};
	REQUIRE_ARRAY_EQ(int, should_result, result, 3, "iterated result and actual result should not be the same");
	ccv_cnnp_dataframe_iter_free(iter);
	ccv_cnnp_dataframe_free(reduce);
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("dataframe map before reduce")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	ccv_cnnp_dataframe_t* const reduce = ccv_cnnp_dataframe_reduce_new(dataframe, _ccv_iter_reduce_int, 0, derived, 3, 0, 0);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(reduce, COLUMN_ID_LIST(0));
	int i;
	void* data;
	int result[3];
	for (i = 0; ; i++)
	{
		if (0 != ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
			break;
		result[i] = (int)(intptr_t)data;
	}
	int should_result[3] = {
		12, 21, 19
	};
	REQUIRE_ARRAY_EQ(int, should_result, result, 3, "iterated result and actual result should not be the same");
	ccv_cnnp_dataframe_iter_free(iter);
	ccv_cnnp_dataframe_free(reduce);
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("dataframe map after reduce")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	ccv_cnnp_dataframe_t* const reduce = ccv_cnnp_dataframe_reduce_new(dataframe, _ccv_iter_reduce_int, 0, 0, 3, 0, 0);
	const int derived = ccv_cnnp_dataframe_map(reduce, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(reduce, COLUMN_ID_LIST(derived));
	int i;
	void* data;
	int result[3];
	for (i = 0; ; i++)
	{
		if (0 != ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
			break;
		result[i] = (int)(intptr_t)data;
	}
	int should_result[3] = {
		10, 19, 18
	};
	REQUIRE_ARRAY_EQ(int, should_result, result, 3, "iterated result and actual result should not be the same");
	ccv_cnnp_dataframe_iter_free(iter);
	ccv_cnnp_dataframe_free(reduce);
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("dataframe map after reduce shuffled")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	ccv_cnnp_dataframe_t* const reduce = ccv_cnnp_dataframe_reduce_new(dataframe, _ccv_iter_reduce_int, 0, 0, 3, 0, 0);
	const int derived = ccv_cnnp_dataframe_map(reduce, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	ccv_cnnp_dataframe_shuffle(reduce);
	assert(derived > 0);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(reduce, COLUMN_ID_LIST(derived));
	int i;
	void* data;
	int result[3];
	for (i = 0; ; i++)
	{
		if (0 != ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
			break;
		result[i] = (int)(intptr_t)data;
	}
	int covered[3] = {};
	for (i = 0; i < 3; i++)
		if (result[i] == 10)
			covered[0] = 1;
		else if (result[i] == 19)
			covered[1] = 1;
		else if (result[i] == 18)
			covered[2] = 1;
	int is_covered[3] = {
		1, 1, 1
	};
	REQUIRE_ARRAY_EQ(int, covered, is_covered, 3, "data should covered all");
	ccv_cnnp_dataframe_iter_free(iter);
	ccv_cnnp_dataframe_free(reduce);
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("iterate through newly added column")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(0, 0, 8);
	const int derived = ccv_cnnp_dataframe_add(dataframe, _ccv_iter_int, 0, 0, int_array, 0);
	assert(derived >= 0);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	int result[8];
	int i = 0;
	void* data;
	while (0 == ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
		result[i++] = (int)(intptr_t)data;
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated result and actual result should be the same");
	// iter2 test some prefetch capacities.
	ccv_cnnp_dataframe_iter_t* const iter2 = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(derived));
	for (i = 0; i < 3; i++)
		ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	_ccv_iter_accessed = 0;
	for (i = 0; i < 3; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "the iterator is not accessed at all, because prefetching");
	ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
	result[3] = (int)(intptr_t)data;
	REQUIRE_EQ(_ccv_iter_accessed, 1, "the iterator is accessed, because no prefetching");
	ccv_cnnp_dataframe_iter_prefetch(iter2, 4, 0);
	REQUIRE_EQ(_ccv_iter_accessed, 2, "the iterator is accessed again, for prefetching");
	_ccv_iter_accessed = 0;
	for (i = 4; i < 6; i++)
	{
		ccv_cnnp_dataframe_iter_next(iter2, &data, 1, 0);
		result[i] = (int)(intptr_t)data;
	}
	REQUIRE_EQ(_ccv_iter_accessed, 0, "the iterator accessed 0 times");
	const int fail0 = ccv_cnnp_dataframe_iter_prefetch(iter2, 1, 0);
	REQUIRE_EQ(fail0, -1, "should fail");
	ccv_cnnp_dataframe_iter_free(iter2);
	REQUIRE_ARRAY_EQ(int, int_array, result, 6, "iterated result and actual result should be the same up to 6");
	ccv_cnnp_dataframe_free(dataframe);
}

TEST_CASE("make a tuple out of column and derived column")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	const int tuple = ccv_cnnp_dataframe_make_tuple(dataframe, COLUMN_ID_LIST(derived, 0));
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(tuple));
	int result[2][8];
	int i = 0;
	void* data;
	while (0 == ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
	{
		void** int_data = (void**)data;
		result[0][i] = (int)(intptr_t)int_data[0];
		result[1][i] = (int)(intptr_t)int_data[1];
		++i;
	}
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_EQ(ccv_cnnp_dataframe_tuple_size(dataframe, tuple), 2, "It should contain two columns");
	ccv_cnnp_dataframe_free(dataframe);
	REQUIRE_ARRAY_EQ(int, int_array, result[1], 8, "iterated tuple should be the same");
	for (i = 0; i < 8; i++)
		++int_array[i];
	REQUIRE_ARRAY_EQ(int, int_array, result[0], 8, "iterated tuple should be the same");
}

TEST_CASE("extract value out of a tuple")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	const int tuple = ccv_cnnp_dataframe_make_tuple(dataframe, COLUMN_ID_LIST(0, derived));
	const int extract = ccv_cnnp_dataframe_extract_tuple(dataframe, tuple, 1);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(extract));
	int result[8];
	int i = 0;
	void* data;
	while (0 == ccv_cnnp_dataframe_iter_next(iter, &data, 1, 0))
		result[i++] = (int)(intptr_t)data;
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_EQ(ccv_cnnp_dataframe_tuple_size(dataframe, tuple), 2, "It should contain two columns");
	ccv_cnnp_dataframe_free(dataframe);
	for (i = 0; i < 8; i++)
		++int_array[i];
	REQUIRE_ARRAY_EQ(int, int_array, result, 8, "iterated tuple should be the same");
}

TEST_CASE("iterate through derived column with peek")
{
	int int_array[8] = {
		2, 3, 4, 5, 6, 7, 8, 9
	};
	ccv_cnnp_column_data_t columns[] = {
		{
			.data_enum = _ccv_iter_int,
			.context = int_array,
		}
	};
	ccv_cnnp_dataframe_t* const dataframe = ccv_cnnp_dataframe_new(columns, sizeof(columns) / sizeof(columns[0]), 8);
	const int derived = ccv_cnnp_dataframe_map(dataframe, _ccv_int_plus_1, 0, 0, COLUMN_ID_LIST(0), 0, 0);
	assert(derived > 0);
	ccv_cnnp_dataframe_iter_t* const iter = ccv_cnnp_dataframe_iter_new(dataframe, COLUMN_ID_LIST(0, derived));
	int result0[8];
	int result1[8];
	int i = 0;
	void* data;
	while (0 == ccv_cnnp_dataframe_iter_next(iter, 0, 0, 0))
	{
		ccv_cnnp_dataframe_iter_peek(iter, &data, 0, 1, 0);
		result0[i] = (int)(intptr_t)data;
		ccv_cnnp_dataframe_iter_peek(iter, &data, 1, 1, 0);
		result1[i] = (int)(intptr_t)data;
		++i;
	}
	ccv_cnnp_dataframe_iter_free(iter);
	REQUIRE_ARRAY_EQ(int, int_array, result0, 8, "iterated result and actual result should be the same");
	for (i = 0; i < 8; i++)
		++int_array[i];
	REQUIRE_ARRAY_EQ(int, int_array, result1, 8, "iterated result and actual result should be the same");
	ccv_cnnp_dataframe_free(dataframe);
}

#include "case_main.h"
