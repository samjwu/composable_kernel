// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

template <typename DeviceInstance>
int run_layernorm4d_fwd_example()
{
    bool time_kernel = false;

    ck::index_t N = 256;
    ck::index_t H = 16;
    ck::index_t W = 16;
    ck::index_t C = 8;

    Tensor<XDataType> x({N, H, W, C});
    Tensor<GammaDataType> gamma({H, W, C});
    Tensor<BetaDataType> beta({H, W, C});
    Tensor<YDataType> y({N, H, W, C});
    Tensor<SaveMeanInvStdDataType> save_mean({N});
    Tensor<SaveMeanInvStdDataType> save_inv_std({N});

    x.GenerateTensorValue(GeneratorTensor_3<XDataType>{0.0, 1.0});
    gamma.GenerateTensorValue(GeneratorTensor_3<GammaDataType>{0.0, 1.0});
    beta.GenerateTensorValue(GeneratorTensor_3<BetaDataType>{0.0, 1.0});

    DeviceMem x_dev(sizeof(XDataType) * x.mDesc.GetElementSpaceSize());
    DeviceMem gamma_dev(sizeof(GammaDataType) * gamma.mDesc.GetElementSpaceSize());
    DeviceMem beta_dev(sizeof(BetaDataType) * beta.mDesc.GetElementSpaceSize());
    DeviceMem y_dev(sizeof(YDataType) * y.mDesc.GetElementSpaceSize());
#ifdef SAVE_MEAN_INV_STD
    DeviceMem save_mean_dev(sizeof(SaveMeanInvStdDataType) * save_mean.mDesc.GetElementSpaceSize());
    DeviceMem save_inv_std_dev(sizeof(SaveMeanInvStdDataType) *
                               save_inv_std.mDesc.GetElementSpaceSize());
#endif

    x_dev.ToDevice(x.mData.data());
    gamma_dev.ToDevice(gamma.mData.data());
    beta_dev.ToDevice(beta.mData.data());

    auto device_instance = DeviceInstance{};
    auto argument_ptr    = device_instance.MakeArgumentPointer(
        {N, H, W, C},
        std::vector<ck::index_t>{x.mDesc.GetStrides().begin(), x.mDesc.GetStrides().end()},
        {0, W * C, C, 1},
        {0, W * C, C, 1},
        std::vector<ck::index_t>{y.mDesc.GetStrides().begin(), y.mDesc.GetStrides().end()},
        std::vector<ck::index_t>{save_mean.mDesc.GetStrides().begin(),
                                    save_mean.mDesc.GetStrides().end()},
        std::vector<ck::index_t>{save_mean.mDesc.GetStrides().begin(),
                                    save_mean.mDesc.GetStrides().end()},
        {1, 2, 3},
        1e-4,
        x_dev.GetDeviceBuffer(),
        gamma_dev.GetDeviceBuffer(),
        beta_dev.GetDeviceBuffer(),
        y_dev.GetDeviceBuffer(),
#ifdef SAVE_MEAN_INV_STD
        save_mean_dev.GetDeviceBuffer(),
        save_inv_std_dev.GetDeviceBuffer(),
#else
        nullptr,
        nullptr,
#endif
        PassThrough{});

    if(!device_instance.IsSupportedArgument(argument_ptr.get()))
    {
        std::cout << "The runtime parameters are not supported" << std::endl;
        return 1;
    };

    size_t workspace_sz = device_instance.GetWorkSpaceSize(argument_ptr.get());
    DeviceMem workspace_dev(workspace_sz);
    device_instance.SetWorkSpacePointer(argument_ptr.get(), workspace_dev.GetDeviceBuffer());

    auto invoker_ptr = device_instance.MakeInvokerPointer();
    invoker_ptr->Run(argument_ptr.get(), StreamConfig{nullptr, time_kernel});

    bool pass = true;
    {
        Tensor<YDataType> host_y({N, H, W, C});
        Tensor<SaveMeanInvStdDataType> host_save_mean({N});
        Tensor<SaveMeanInvStdDataType> host_save_inv_std({N});

        using ReferenceInstance =
            ck::tensor_operation::host::ReferenceLayernorm<XDataType,
                                                           GammaDataType,
                                                           BetaDataType,
                                                           YDataType,
                                                           SaveMeanInvStdDataType,
                                                           ComputeDataType,
                                                           PassThrough,
                                                           Rank,
                                                           NumReduceDim>;

        ReferenceInstance ref;
        auto ref_argument = ref.MakeArgument(x,
                                             gamma,
                                             beta,
                                             host_y,
                                             host_save_mean,
                                             host_save_inv_std,
                                             PassThrough{},
                                             {N, H, W, C},
                                             {1, 2, 3},
                                             1e-4);
        auto ref_invoker  = ref.MakeInvoker();
        ref_invoker.Run(ref_argument);

        y_dev.FromDevice(y.mData.data());
        pass &= ck::utils::check_err(y, host_y, "Error: Incorrect results (y)", 1e-3, 1e-3);
#ifdef SAVE_MEAN_INV_STD
        save_mean_dev.FromDevice(save_mean.mData.data());
        save_inv_std_dev.FromDevice(save_inv_std.mData.data());
        pass &= ck::utils::check_err(
            save_mean, host_save_mean, "Error: Incorrect results (mean)", 1e-3, 1e-3);
        pass &= ck::utils::check_err(
            save_inv_std, host_save_inv_std, "Error: Incorrect results (inv_std)", 1e-3, 1e-3);
#endif
    }

    return (pass ? 0 : 1);
}
