#ifndef DEVICE_REDUCE_INSTANCE_MULTIBLOCK_ATOMIC_ADD_HPP
#define DEVICE_REDUCE_INSTANCE_MULTIBLOCK_ATOMIC_ADD_HPP

#include "reduction_operator_mapping.hpp"
#include "device_reduce_instance_impl_common.hpp"
#include "device_reduce_multiblock_atomic_add.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace device_reduce_instance {

#ifdef QUICK_REDUCE_TEST
using reduce_configuration_2_instances_multiblock_atomic_add = std::tuple<
    // clang-format off
    // InSrcVectorDim | InSrcVectorSize | OutDstVectorSize | MThreadSliceSize | KThreadSliceSize
    ReductionConfiguration_2<0, 2, 2, 2, 1>,
    ReductionConfiguration_2<0, 1, 1, 2, 1>,
    ReductionConfiguration_2<1, 2, 1, 1, 2>,
    ReductionConfiguration_2<1, 2, 2, 1, 2>,
    ReductionConfiguration_2<0, 1, 1, 3, 1>,
    ReductionConfiguration_2<1, 1, 1, 1, 3>
    // clang-format on
    >;
#else
using reduce_configuration_2_instances_multiblock_atomic_add = std::tuple<
    // clang-format off
    // InSrcVectorDim | InSrcVectorSize | OutDstVectorSize | MThreadSliceSize | KThreadSliceSize
    ReductionConfiguration_2<0, 4, 4, 8, 1>,
    ReductionConfiguration_2<0, 4, 4, 4, 1>,
    ReductionConfiguration_2<0, 2, 2, 2, 1>,

    ReductionConfiguration_2<1, 4, 1, 1, 8>,
    ReductionConfiguration_2<1, 4, 1, 1, 4>,
    ReductionConfiguration_2<1, 2, 1, 1, 2>,

    // special instances
    ReductionConfiguration_2<0, 1, 1, 3, 1>,
    ReductionConfiguration_2<0, 1, 1, 5, 1>,
    ReductionConfiguration_2<0, 1, 1, 7, 1>,
    ReductionConfiguration_2<0, 1, 1, 11, 1>,

    ReductionConfiguration_2<1, 1, 1, 1, 3>,
    ReductionConfiguration_2<1, 1, 1, 1, 5>,
    ReductionConfiguration_2<1, 1, 1, 1, 7>,
    ReductionConfiguration_2<1, 1, 1, 1, 11>
    // clang-format on
    >;
#endif

template <typename AccDataType, ReduceTensorOp_t ReduceOperation>
using deviceReduceMultiBlockAtomicAddPtrType =
    DeviceReducePtr<typename reduce_unary_operator<AccDataType, ReduceOperation, true, true>::
                        InElementwiseOperation,
                    typename reduce_unary_operator<AccDataType, ReduceOperation, true, true>::
                        AccElementwiseOperation>;

template <typename InDataType,
          typename AccDataType,
          typename OutDataType,
          int Rank,
          typename ReduceDims,
          ReduceTensorOp_t ReduceOpId,
          NanPropagation_t NanOpt,
          ReduceTensorIndices_t IndicesOpt>
void add_device_reduce_instance_multiblock_atomic_add(
    std::vector<deviceReduceMultiBlockAtomicAddPtrType<AccDataType, ReduceOpId>>&
        device_op_instances)
{
    using ReduceOperation = typename reduce_binary_operator<AccDataType, ReduceOpId>::opType;
    using InElementwiseOperation =
        typename reduce_unary_operator<AccDataType, ReduceOpId, true, true>::InElementwiseOperation;
    using AccElementwiseOperation =
        typename reduce_unary_operator<AccDataType, ReduceOpId, true, true>::
            AccElementwiseOperation;

    constexpr bool Indexable =
        (ReduceOpId == ReduceTensorOp_t::MIN || ReduceOpId == ReduceTensorOp_t::MAX ||
         ReduceOpId == ReduceTensorOp_t::AMAX);
    constexpr bool NeedIndices = Indexable && (IndicesOpt != ReduceTensorIndices_t::NO_INDICES);

    constexpr bool PropagateNan = (NanOpt == NanPropagation_t::NOT_PROPAGATE_NAN) ? false : true;

    static_assert(IndicesOpt == ReduceTensorIndices_t::NO_INDICES,
                  "AtomicAdd can only be used with reduction operations without indices!");

    constexpr bool op_acceptable =
        (ReduceOpId == ReduceTensorOp_t::ADD || ReduceOpId == ReduceTensorOp_t::MUL ||
         ReduceOpId == ReduceTensorOp_t::AVG || ReduceOpId == ReduceTensorOp_t::NORM1);

    constexpr bool out_type_acceptable =
        (std::is_same<OutDataType, float>::value || std::is_same<OutDataType, double>::value);

    if constexpr(!op_acceptable || !out_type_acceptable)
        return;
    else
    {
        static_for<0, std::tuple_size<reduce_configuration_1_instances>::value, 1>{}([&](auto i) {
            using cfg1 =
                remove_cvref_t<decltype(std::get<i.value>(reduce_configuration_1_instances{}))>;

            static_for<
                0,
                std::tuple_size<reduce_configuration_2_instances_multiblock_atomic_add>::value,
                1>{}([&](auto j) {
                using cfg2 = remove_cvref_t<decltype(
                    std::get<j.value>(reduce_configuration_2_instances_multiblock_atomic_add{}))>;

                using ReduceOpInstance = DeviceReduceMultiBlockAtomicAdd<InDataType,
                                                                         AccDataType,
                                                                         OutDataType,
                                                                         Rank,
                                                                         ReduceDims,
                                                                         ReduceOperation,
                                                                         InElementwiseOperation,
                                                                         AccElementwiseOperation,
                                                                         PropagateNan,
                                                                         NeedIndices,
                                                                         cfg1::BlockSize_,
                                                                         cfg1::MThreadClusterSize_,
                                                                         cfg1::KThreadClusterSize_,
                                                                         cfg2::MThreadSliceSize_,
                                                                         cfg2::KThreadSliceSize_,
                                                                         cfg2::InSrcVectorDim_,
                                                                         cfg2::InSrcVectorSize_,
                                                                         cfg2::OutDstVectorSize_>;

                device_op_instances.push_back(
                    std::make_unique<ReduceOpInstance>(ReduceOpInstance{}));
            });
        });
    }
};

#define ADD_MULTIBLOCK_ATOMIC_ADD_INST_BY_TYPE(                                           \
    inT, compT, outT, ReduceOpId, NanOpt, IndicesOpt, Rank, ...)                          \
    template void add_device_reduce_instance_multiblock_atomic_add<inT,                   \
                                                                   compT,                 \
                                                                   outT,                  \
                                                                   Rank,                  \
                                                                   Sequence<__VA_ARGS__>, \
                                                                   ReduceOpId,            \
                                                                   NanOpt,                \
                                                                   IndicesOpt>(           \
        std::vector<deviceReduceMultiBlockAtomicAddPtrType<compT, ReduceOpId>> &          \
        device_op_instances)

#define ADD_MULTIBLOCK_ATOMIC_ADD_INST_BY_ID(                                              \
    inT, compT, outT, ReduceOpId, NanOpt, IndicesOpt, Rank, ...)                           \
    ADD_MULTIBLOCK_ATOMIC_ADD_INST_BY_TYPE(inT,                                            \
                                           compT,                                          \
                                           outT,                                           \
                                           static_cast<ReduceTensorOp_t>(ReduceOpId),      \
                                           static_cast<NanPropagation_t>(NanOpt),          \
                                           static_cast<ReduceTensorIndices_t>(IndicesOpt), \
                                           Rank,                                           \
                                           __VA_ARGS__)

#define ADD_MULTIBLOCK_ATOMIC_ADD_INST_REF_BY_TYPE(                                                \
    inT, compT, outT, ReduceOpId, NanOpt, IndicesOpt, Rank, ...)                                   \
    extern template void add_device_reduce_instance_multiblock_atomic_add<inT,                     \
                                                                          compT,                   \
                                                                          outT,                    \
                                                                          Rank,                    \
                                                                          Sequence<__VA_ARGS__>,   \
                                                                          ReduceOpId,              \
                                                                          NanOpt,                  \
                                                                          IndicesOpt>(             \
        std::vector<DeviceReducePtr<                                                               \
            typename reduce_unary_operator<compT, ReduceOpId, true, true>::InElementwiseOperation, \
            typename reduce_unary_operator<compT, ReduceOpId, true, true>::                        \
                AccElementwiseOperation>> &                                                        \
        device_op_instances)

#define ADD_MULTIBLOCK_ATOMIC_ADD_INST_REF_BY_ID(                                              \
    inT, compT, outT, ReduceOpId, NanOpt, IndicesOpt, Rank, ...)                               \
    ADD_MULTIBLOCK_ATOMIC_ADD_INST_REF_BY_TYPE(inT,                                            \
                                               compT,                                          \
                                               outT,                                           \
                                               static_cast<ReduceTensorOp_t>(ReduceOpId),      \
                                               static_cast<NanPropagation_t>(NanOpt),          \
                                               static_cast<ReduceTensorIndices_t>(IndicesOpt), \
                                               Rank,                                           \
                                               __VA_ARGS__)

} // namespace device_reduce_instance
} // namespace device
} // namespace tensor_operation

} // namespace ck

#endif