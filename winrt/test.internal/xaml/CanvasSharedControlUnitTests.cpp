// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may
// not use these files except in compliance with the License. You may obtain
// a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#include "pch.h"

#define TEST_SHARED_CONTROL_BEHAVIOR(NAME)          \
    TEST_METHOD_EX(CanvasControl_##NAME)            \
    {                                               \
        NAME<CanvasControlTraits>();                \
    }                                               \
                                                    \
    TEST_METHOD_EX(CanvasAnimatedControl_##NAME)    \
    {                                               \
        NAME<CanvasAnimatedControlTraits>();        \
    }                                               \
                                                    \
    template<typename TRAITS>                       \
    void NAME()



TEST_CLASS(CanvasSharedControlTests_ClearColor)
{
    TEST_SHARED_CONTROL_BEHAVIOR(DefaultClearColorIsTransparentBlack)
    {
        ClearColorFixture<TRAITS> f;

        Color expectedColor{ 0, 0, 0, 0 };
        
        Color actualColor{ 0xFF, 0xFF, 0xFF, 0xFF };
        
        ThrowIfFailed(f.Control->get_ClearColor(&actualColor));
        
        Assert::AreEqual(expectedColor, actualColor);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(ClearColorValueIsPersisted)
    {
        ClearColorFixture<TRAITS> f;
        
        Color anyColor{ 1, 2, 3, 4 };
        
        ThrowIfFailed(f.Control->put_ClearColor(anyColor));
        
        Color actualColor{ 0xFF, 0xFF, 0xFF, 0xFF };
        ThrowIfFailed(f.Control->get_ClearColor(&actualColor));

        Assert::AreEqual(anyColor, actualColor);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(DeviceContextIsClearedToCorrectColorBeforeDrawHandlerIsCalled)
    {
        ClearColorFixture<TRAITS> f;
        f.RegisterOnDraw();

        Color anyColor{ 1, 2, 3, 4 };

        f.OnDraw.SetExpectedCalls(0);

        f.DeviceContext->ClearMethod.ExpectAtLeastOneCall(
            [&](D2D1_COLOR_F const* color)
            {
                Assert::AreEqual(ToD2DColor(anyColor), *color);
                f.OnDraw.SetExpectedCalls(1);
            });

        ThrowIfFailed(f.Control->put_ClearColor(anyColor));

        f.Load();
        f.RenderAnyNumberOfFrames();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(DeviceContextIsClearedToCorrectColorEvenIfNoDrawHandlersRegistered)
    {
        ClearColorFixture<TRAITS> f;

        Color anyColor{ 1, 2, 3, 4 };

        f.DeviceContext->ClearMethod.ExpectAtLeastOneCall(
            [&](D2D1_COLOR_F const* color)
            {
                Assert::AreEqual(ToD2DColor(anyColor), *color);
            });
        
        ThrowIfFailed(f.Control->put_ClearColor(anyColor));

        f.Load();
        f.RenderAnyNumberOfFrames();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(DeviceContextIsClearedToCorrectColorWhenColorChanges)
    {
        ClearColorFixture<TRAITS> f;

        Color anyColor{ 1, 2, 3, 4 };

        f.DeviceContext->ClearMethod.ExpectAtLeastOneCall(
            [&](D2D1_COLOR_F const* color)
            {
                Assert::AreEqual(ToD2DColor(anyColor), *color);
            });
        
        ThrowIfFailed(f.Control->put_ClearColor(anyColor));

        f.Load();
        f.RenderAnyNumberOfFrames();

        Color anyOtherColor{ 5, 6, 7, 8 };

        f.DeviceContext->ClearMethod.ExpectAtLeastOneCall(
            [&](D2D1_COLOR_F const* color)
            {
                Assert::AreEqual(ToD2DColor(anyOtherColor), *color);
            });

        ThrowIfFailed(f.Control->put_ClearColor(anyOtherColor));

        f.RenderAnyNumberOfFrames();
    }

};

TEST_CLASS(CanvasSharedControlTests_InteractionWithRecreatableDeviceManager)
{
    TEST_SHARED_CONTROL_BEHAVIOR(WhenDpiChangedEventRaised_ForwardsToDeviceManager)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;
        f.Load();

        // DPI change event without an actual change to the value should be ignored.
        f.Adapter->RaiseDpiChangedEvent();

        // But if the value changes, this should be passed on to the device manager.
        f.Adapter->LogicalDpi = 100;

        f.DeviceManager->SetDpiChangedMethod.SetExpectedCalls(1);
        f.Adapter->RaiseDpiChangedEvent();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(WhenControlNotLoaded_DpiChangedEventIsIgnored)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        f.DeviceManager->SetDpiChangedMethod.SetExpectedCalls(0);

        // DPI change events on a new, not yet loaded control should be ignored.
        f.Adapter->LogicalDpi = 100;
        f.Adapter->RaiseDpiChangedEvent();

        f.Adapter->LogicalDpi = 96;
        f.Adapter->RaiseDpiChangedEvent();

        // Ditto after loading and then unloading the control again.
        f.Load();
        f.RaiseUnloadedEvent();

        f.Adapter->LogicalDpi = 100;
        f.Adapter->RaiseDpiChangedEvent();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(WhenDpiChangesWhileControlNotLoaded_LoadRaisesDpiChangedEvent)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        // DPI changes, but is ignored because the control is not yet loaded.
        f.Adapter->LogicalDpi = 100;
        f.Adapter->RaiseDpiChangedEvent();

        // When the control loads, the DPI change shoudl be picked up.
        f.DeviceManager->SetDpiChangedMethod.SetExpectedCalls(1);
        f.Load();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(add_CreateResources_ForwardsToDeviceManager)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        EventRegistrationToken anyToken{0x1234567890ABCDEF};
        auto anyHandler = Callback<ControlFixtureWithRecreatableDeviceManager<TRAITS>::createResourcesEventHandler_t>(
            [](typename TRAITS::controlInterface_t*, IInspectable*) { return S_OK; } );

        f.DeviceManager->AddCreateResourcesMethod.SetExpectedCalls(1,
            [&](typename TRAITS::control_t* control, typename TRAITS::createResourcesEventHandler_t* handler)
            {
                Assert::IsTrue(IsSameInstance(f.Control.Get(), control));
                Assert::IsTrue(IsSameInstance(anyHandler.Get(), handler));
                return anyToken;
            });

        EventRegistrationToken actualToken;
        ThrowIfFailed(f.Control->add_CreateResources(anyHandler.Get(), &actualToken));

        Assert::AreEqual(anyToken.value, actualToken.value);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(remove_CreateResources_ForwardsToDeviceManager)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        EventRegistrationToken anyToken{0x1234567890ABCDEF};
        f.DeviceManager->RemoveCreateResourcesMethod.SetExpectedCalls(1,
            [&](EventRegistrationToken token)
            {
                Assert::AreEqual(anyToken.value, token.value);
            });

        ThrowIfFailed(f.Control->remove_CreateResources(anyToken));
    }

    TEST_SHARED_CONTROL_BEHAVIOR(get_ReadyToDraw_ForwardsToDeviceManager)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        f.DeviceManager->IsReadyToDrawMethod.SetExpectedCalls(1,
            [] { return true; });

        boolean value;
        ThrowIfFailed(f.Control->get_ReadyToDraw(&value));
        Assert::IsTrue(!!value);

        f.DeviceManager->IsReadyToDrawMethod.SetExpectedCalls(1,
            [] { return false; });

        ThrowIfFailed(f.Control->get_ReadyToDraw(&value));
        Assert::IsFalse(!!value);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(get_Device_ForwardsToDeviceManager)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        auto anyDevice = Make<MockCanvasDevice>();
        f.DeviceManager->SetDevice(anyDevice);

        ComPtr<ICanvasDevice> device;
        ThrowIfFailed(f.Control->get_Device(&device));

        Assert::IsTrue(IsSameInstance(anyDevice.Get(), device.Get()));
    }

    TEST_SHARED_CONTROL_BEHAVIOR(WhenGetDeviceReturnsNull_get_Device_ReportsUnderstandableErrorMessage)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        ComPtr<ICanvasDevice> device;
        Assert::AreEqual(E_INVALIDARG, f.Control->get_Device(&device));

        ValidateStoredErrorState(E_INVALIDARG, Strings::CanvasDeviceGetDeviceWhenNotCreated);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(WhenDrawing_RunWithDrawIsPassedTheCorrectControl)
    {
        ControlFixtureWithRecreatableDeviceManager<TRAITS> f;

        f.DeviceManager->RunWithDeviceMethod.SetExpectedCalls(1,
            [&](typename TRAITS::control_t* control, DeviceCreationOptions, RunWithDeviceFunction)
            {
                Assert::IsTrue(IsSameInstance(f.Control.Get(), control));
            });

        f.Load();
        f.EnsureChangedCallback();
        f.RenderSingleFrame();
    }

    template<typename TRAITS>
    struct DrawFixture : public ControlFixtureWithRecreatableDeviceManager<TRAITS>
    {
        ComPtr<StubCanvasDevice> Device;
        MockEventHandler<typename TRAITS::drawEventHandler_t> OnDraw;

        DrawFixture()
            : Device(Make<StubCanvasDevice>())
            , OnDraw(L"OnDraw")
        {
            PrepareAdapter<TRAITS>();

            AddDrawHandler(OnDraw.Get());

            DeviceManager->SetDevice(Device);
        }

        template<typename T>
        void PrepareAdapter() {}

        template<>
        void PrepareAdapter<CanvasControlTraits>()
        {
            Adapter->CreateCanvasImageSourceMethod.AllowAnyCall();
        }

        template<>
        void PrepareAdapter<CanvasAnimatedControlTraits>()
        {
            auto swapChainManager = std::make_shared<MockCanvasSwapChainManager>();

            Adapter->CreateCanvasSwapChainMethod.AllowAnyCall(
                [=] (ICanvasDevice*, float, float, float, CanvasAlphaMode)
                { 
                    auto mockSwapChain = swapChainManager->CreateMock(); 
                    mockSwapChain->CreateDrawingSessionMethod.AllowAnyCall(
                        [] (Color, ICanvasDrawingSession** value)
                        {
                            auto ds = Make<MockCanvasDrawingSession>();
                            return ds.CopyTo(value);
                        });
                    mockSwapChain->PresentMethod.AllowAnyCall();

                    mockSwapChain->put_TransformMethod.AllowAnyCall();

                    return mockSwapChain;
                });
        }

        void EnsureChangedCallback()
        {
            EnsureChangedCallbackImpl<TRAITS>();
        }

    private:

        template<typename T>
        void EnsureChangedCallbackImpl() {}

        template<>
        void EnsureChangedCallbackImpl<CanvasAnimatedControlTraits>()
        {
            Adapter->DoChanged();
        }
    };


    TEST_SHARED_CONTROL_BEHAVIOR(WhenResourcesAreNotCreated_DrawHandlersAreNotCalled)
    {
        DrawFixture<TRAITS> f;

        f.OnDraw.SetExpectedCalls(0);
        f.DeviceManager->SetRunWithDeviceFlags(RunWithDeviceFlags::ResourcesNotCreated, 1);

        f.Load();
        f.EnsureChangedCallback();
        f.RenderSingleFrame();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(WhenResourcesAreCreated_DrawHandlersAreCalled)
    {
        DrawFixture<TRAITS> f;

        f.OnDraw.SetExpectedCalls(1);
        f.DeviceManager->SetRunWithDeviceFlags(RunWithDeviceFlags::None, 1);

        f.Load();
        f.EnsureChangedCallback();
        f.RenderSingleFrame();
    }    
};

TEST_CLASS(CanvasSharedControlTests_CommonAdapter)
{
    TEST_SHARED_CONTROL_BEHAVIOR(DpiProperties)
    {
        BasicControlFixture<TRAITS> f;

        const float dpi = 144;

        f.CreateAdapter();

        f.Adapter->LogicalDpi = dpi;
        
        f.CreateControl();

        float actualDpi = 0;
        ThrowIfFailed(f.Control->get_Dpi(&actualDpi));
        Assert::AreEqual(dpi, actualDpi);

        const float testValue = 100;

        int pixels = 0;
        ThrowIfFailed(f.Control->ConvertDipsToPixels(testValue, &pixels));
        Assert::AreEqual((int)(testValue * dpi / DEFAULT_DPI), pixels);

        float dips = 0;
        ThrowIfFailed(f.Control->ConvertPixelsToDips((int)testValue, &dips));
        Assert::AreEqual(testValue * DEFAULT_DPI / dpi, dips);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(WhenLoadedAndUnloaded_EventsAreRegisteredAndUnregistered)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();

        // Creating the control should not register events.
        f.Adapter->DpiChangedEventSource->AddMethod.SetExpectedCalls(0);
        f.Adapter->SuspendingEventSource->AddMethod.SetExpectedCalls(0);
        f.Adapter->ResumingEventSource->AddMethod.SetExpectedCalls(0);

        f.Adapter->DpiChangedEventSource->RemoveMethod.SetExpectedCalls(0);
        f.Adapter->SuspendingEventSource->RemoveMethod.SetExpectedCalls(0);
        f.Adapter->ResumingEventSource->RemoveMethod.SetExpectedCalls(0);

        f.CreateControl();

        // Loading the control should register events.
        f.Adapter->DpiChangedEventSource->AddMethod.SetExpectedCalls(1);
        f.Adapter->SuspendingEventSource->AddMethod.SetExpectedCalls(1);
        f.Adapter->ResumingEventSource->AddMethod.SetExpectedCalls(1);

        f.Load();

        Expectations::Instance()->Validate();

        // Unloading the control should unregister events.
        f.Adapter->DpiChangedEventSource->RemoveMethod.SetExpectedCalls(1);
        f.Adapter->SuspendingEventSource->RemoveMethod.SetExpectedCalls(1);
        f.Adapter->ResumingEventSource->RemoveMethod.SetExpectedCalls(1);

        f.RaiseUnloadedEvent();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(SizeProperty)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();
        f.CreateControl();

        Size size{ -1, -1 };
        ThrowIfFailed(f.Control->get_Size(&size));
        Assert::AreEqual(Size{ 0, 0 }, size);

        Size newSize{ 123, 456 };
        f.UserControl->Resize(newSize);

        ThrowIfFailed(f.Control->get_Size(&size));
        Assert::AreEqual(newSize, size);
    }
};

CanvasHardwareAcceleration hardwareAccelerationOptions[] =
{
    CanvasHardwareAcceleration::Auto,
    CanvasHardwareAcceleration::On,
    CanvasHardwareAcceleration::Off
};

template<typename TRAITS>
struct DeviceCreationFixture : public BasicControlFixture<TRAITS>
{
    RecreatableDeviceManager<TRAITS>* LastDeviceManager;

    DeviceCreationFixture()
    {
        CreateAdapter();

        Adapter->CreateRecreatableDeviceManagerMethod.AllowAnyCall(
            [=]
            {
                auto manager = std::make_unique<RecreatableDeviceManager<TRAITS>>(Adapter->DeviceFactory.Get());

                LastDeviceManager = manager.get();

                return manager;
            });

        CreateControl();
        PrepareAdapterForRenderingResource();
        Load();
    }
    void ExpectOneGetSharedDevice()
    {
        Adapter->DeviceFactory->GetSharedDeviceMethod.SetExpectedCalls(1,
            [&](CanvasHardwareAcceleration hardwareAcceleration, ICanvasDevice** device)
        {
            Make<StubCanvasDevice>().CopyTo(device);
            return S_OK;
        });
    }

    void DoNotExpectGetSharedDevice()
    {
        Adapter->DeviceFactory->GetSharedDeviceMethod.SetExpectedCalls(0);
    }

    void ExpectOneCreateDevice()
    {
        Adapter->DeviceFactory->CreateWithDebugLevelAndHardwareAccelerationMethod.SetExpectedCalls(1,
            [&](CanvasDebugLevel, CanvasHardwareAcceleration hardwareAcceleration, ICanvasDevice** device)
        {
            Make<StubCanvasDevice>().CopyTo(device);
            return S_OK;
        });
    }

    void DoNotExpectCreateDevice()
    {
        Adapter->DeviceFactory->CreateWithDebugLevelAndHardwareAccelerationMethod.SetExpectedCalls(0);
    }

    void DoNotExpectChanged()
    {
        LastDeviceManager->SetChangedCallback(
                [=] (ChangeReason reason)
                {
                    Assert::Fail();
                });
    }

    void EnsureDevice();
};

inline void DeviceCreationFixture<CanvasControlTraits>::EnsureDevice()
{
    RenderSingleFrame();
}

inline void DeviceCreationFixture<CanvasAnimatedControlTraits>::EnsureDevice()
{
    Adapter->Tick();
    Adapter->DoChanged();
}

TEST_CLASS(CanvasSharedControlTests_DeviceApis)
{    
    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_Null)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();
        f.CreateControl();

        Assert::AreEqual(E_INVALIDARG, f.Control->get_UseSharedDevice(nullptr));
    }

    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_SetAndGet)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();
        f.CreateControl();

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(false));

        boolean useSharedDevice;
        Assert::AreEqual(S_OK, f.Control->get_UseSharedDevice(&useSharedDevice));

        Assert::IsFalse(!!useSharedDevice);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_IfTrue_CallsGetSharedDevice)
    {
        DeviceCreationFixture<TRAITS> f;

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(true));

        f.ExpectOneGetSharedDevice();
        f.DoNotExpectCreateDevice();
        f.EnsureDevice();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_IfFalse_DoesNotCallGetSharedDevice)
    {
        DeviceCreationFixture<TRAITS> f;

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(false));

        f.DoNotExpectGetSharedDevice();
        f.ExpectOneCreateDevice();
        f.EnsureDevice();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(HardwareAcceleration_Null)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();
        f.CreateControl();

        Assert::AreEqual(E_INVALIDARG, f.Control->get_HardwareAcceleration(nullptr));
    }

    TEST_SHARED_CONTROL_BEHAVIOR(HardwareAcceleration_UnknownIsInvalid)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();
        f.CreateControl();

        Assert::AreEqual(E_INVALIDARG, f.Control->put_HardwareAcceleration(CanvasHardwareAcceleration::Unknown));
        ValidateStoredErrorState(E_INVALIDARG, Strings::HardwareAccelerationUnknownIsNotValid);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(HardwareAcceleration_Default_On)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();
        f.CreateControl();

        CanvasHardwareAcceleration hardwareAcceleration;
        Assert::AreEqual(S_OK, f.Control->get_HardwareAcceleration(&hardwareAcceleration));
        Assert::AreEqual(CanvasHardwareAcceleration::On, hardwareAcceleration);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(HardwareAcceleration_SetAndGet)
    {
        BasicControlFixture<TRAITS> f;

        f.CreateAdapter();
        f.CreateControl();

        Assert::AreEqual(S_OK, f.Control->put_HardwareAcceleration(CanvasHardwareAcceleration::Off));

        CanvasHardwareAcceleration hardwareAcceleration;
        Assert::AreEqual(S_OK, f.Control->get_HardwareAcceleration(&hardwareAcceleration));

        Assert::AreEqual(CanvasHardwareAcceleration::Off, hardwareAcceleration);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_IfTrue_DeviceIsRetrievedWithCorrectOption)
    {
        for (auto expectedHardwareAcceleration : hardwareAccelerationOptions)
        {
            DeviceCreationFixture<TRAITS> f;

            Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(true));
            Assert::AreEqual(S_OK, f.Control->put_HardwareAcceleration(expectedHardwareAcceleration));

            auto stubDevice = Make<StubCanvasDevice>();

            f.Adapter->DeviceFactory->GetSharedDeviceMethod.SetExpectedCalls(1,
                [&](CanvasHardwareAcceleration hardwareAcceleration, ICanvasDevice** device)
                {
                    Assert::AreEqual(expectedHardwareAcceleration, hardwareAcceleration);
                    return stubDevice.CopyTo(device);
                });

            f.EnsureDevice();
        }
    }

    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_IfFalse_DeviceIsCreatedWithCorrectOption)
    {
        for (auto expectedHardwareAcceleration : hardwareAccelerationOptions)
        {
            DeviceCreationFixture<TRAITS> f;
            Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(false));
            Assert::AreEqual(S_OK, f.Control->put_HardwareAcceleration(expectedHardwareAcceleration));
            
            auto stubDevice = Make<StubCanvasDevice>();

            f.Adapter->DeviceFactory->CreateWithDebugLevelAndHardwareAccelerationMethod.SetExpectedCalls(1,
                [&](CanvasDebugLevel, CanvasHardwareAcceleration hardwareAcceleration, ICanvasDevice** device)
                {
                    Assert::AreEqual(expectedHardwareAcceleration, hardwareAcceleration);
                    return stubDevice.CopyTo(device);
                });

            f.EnsureDevice();
        }
    }

    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_ChangingOptionCausesReCreation)
    {
        DeviceCreationFixture<TRAITS> f;

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(true));

        f.ExpectOneGetSharedDevice();
        f.EnsureDevice();

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(false));

        f.ExpectOneCreateDevice();
        f.EnsureDevice();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(UseSharedDevice_RedundantChangesDoNotCauseReCreation)
    {
        DeviceCreationFixture<TRAITS> f;

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(true));

        f.ExpectOneGetSharedDevice();
        f.EnsureDevice();

        for (int i = 0; i < 3; ++i)
        {
            Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(true));
            f.DoNotExpectGetSharedDevice();
            f.EnsureDevice();
        }
    }

    TEST_SHARED_CONTROL_BEHAVIOR(HardwareAcceleration_ChangingOptionCausesReCreation)
    {
        DeviceCreationFixture<TRAITS> f;

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(true));
        Assert::AreEqual(S_OK, f.Control->put_HardwareAcceleration(CanvasHardwareAcceleration::On));

        f.ExpectOneGetSharedDevice();
        f.EnsureDevice();

        Assert::AreEqual(S_OK, f.Control->put_HardwareAcceleration(CanvasHardwareAcceleration::Off));

        f.ExpectOneGetSharedDevice();
        f.EnsureDevice();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(HardwareAcceleration_RedundantChangesDoNotCauseReCreation)
    {
        DeviceCreationFixture<TRAITS> f;

        Assert::AreEqual(S_OK, f.Control->put_UseSharedDevice(true));
        Assert::AreEqual(S_OK, f.Control->put_HardwareAcceleration(CanvasHardwareAcceleration::Off));

        f.ExpectOneGetSharedDevice();
        f.EnsureDevice();

        for (int i = 0; i < 3; ++i)
        {
            Assert::AreEqual(S_OK, f.Control->put_HardwareAcceleration(CanvasHardwareAcceleration::Off));
            f.DoNotExpectGetSharedDevice();
            f.EnsureDevice();
        }
    }

    TEST_SHARED_CONTROL_BEHAVIOR(HardwareAcceleration_get_Device_SucceedsAfterCreateResourcesOccurs)
    {
        DeviceCreationFixture<TRAITS> f;

        MockEventHandler<TRAITS::createResourcesEventHandler_t> onCreateResources;
        EventRegistrationToken token;
        Assert::AreEqual(S_OK, f.Control->add_CreateResources(onCreateResources.Get(), &token));

        onCreateResources.SetExpectedCalls(1);
        f.EnsureDevice();
        Expectations::Instance()->Validate();

        ComPtr<ICanvasDevice> device;
        Assert::AreEqual(S_OK, f.Control->get_Device(&device));
    }
};

TEST_CLASS(CanvasSharedControlTests_CustomDevice)
{
    TEST_SHARED_CONTROL_BEHAVIOR(put_CustomDevice_Null_NothingBadHappens)
    {
        DeviceCreationFixture<TRAITS> f;

        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(nullptr));
    }

    TEST_SHARED_CONTROL_BEHAVIOR(get_CustomDevice_DefaultIsNull)
    {
        DeviceCreationFixture<TRAITS> f;

        ComPtr<ICanvasDevice> device;
        Assert::AreEqual(S_OK, f.Control->get_CustomDevice(&device));
        Assert::IsNull(device.Get());
    }

    TEST_SHARED_CONTROL_BEHAVIOR(GetCustomDevice_put_And_get)
    {
        for (int i = 0; i < 2; ++i)
        {
            DeviceCreationFixture<TRAITS> f;

            // Verify behavior regardless of whether the device resources
            // have been created.
            if (i == 0) f.EnsureDevice();

            auto stubDevice = Make<StubCanvasDevice>();
            Assert::AreEqual(S_OK, f.Control->put_CustomDevice(stubDevice.Get()));

            ComPtr<ICanvasDevice> retrievedDevice;
            Assert::AreEqual(S_OK, f.Control->get_CustomDevice(&retrievedDevice));

            Assert::IsTrue(IsSameInstance(stubDevice.Get(), retrievedDevice.Get()));
        }
    }

    TEST_SHARED_CONTROL_BEHAVIOR(put_CustomDevice_ControlDoesntHaveResourcesYet_Then_get_Device_StillNotAvailable)
    {
        DeviceCreationFixture<TRAITS> f;

        auto stubDevice = Make<StubCanvasDevice>();
        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(stubDevice.Get()));

        ComPtr<ICanvasDevice> retrievedDevice;
        Assert::AreEqual(E_INVALIDARG, f.Control->get_Device(&retrievedDevice));
        ValidateStoredErrorState(E_INVALIDARG, Strings::CanvasDeviceGetDeviceWhenNotCreated);
    }

    TEST_SHARED_CONTROL_BEHAVIOR(put_CustomDevice_ControlHasResources_AndThenOnSameTick_get_Device_StillReturnsTheNonCustomDevice)
    {
        DeviceCreationFixture<TRAITS> f;

        f.EnsureDevice();

        auto stubDevice = Make<StubCanvasDevice>();
        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(stubDevice.Get()));

        ComPtr<ICanvasDevice> retrievedDevice;
        Assert::AreEqual(S_OK, f.Control->get_Device(&retrievedDevice));

        Assert::AreNotEqual(static_cast<ICanvasDevice*>(stubDevice.Get()), retrievedDevice.Get());
    }

    TEST_SHARED_CONTROL_BEHAVIOR(CustomDevice_ReturnedBy_get_Device_AfterNextTick)
    {
        DeviceCreationFixture<TRAITS> f;

        f.EnsureDevice();

        auto customDevice = Make<StubCanvasDevice>();
        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(customDevice.Get()));

        f.EnsureDevice();

        ComPtr<ICanvasDevice> retrievedDevice;
        Assert::AreEqual(S_OK, f.Control->get_Device(&retrievedDevice));

        Assert::AreEqual(static_cast<ICanvasDevice*>(customDevice.Get()), retrievedDevice.Get());
    }

    TEST_SHARED_CONTROL_BEHAVIOR(put_CustomDevice_CanReplaceOtherCustomDevice)
    {
        DeviceCreationFixture<TRAITS> f;

        auto customDevice1 = Make<StubCanvasDevice>();
        auto customDevice2 = Make<StubCanvasDevice>();
        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(customDevice1.Get()));

        f.EnsureDevice();

        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(customDevice2.Get()));

        f.EnsureDevice();

        ComPtr<ICanvasDevice> retrievedDevice;
        Assert::AreEqual(S_OK, f.Control->get_Device(&retrievedDevice));

        Assert::AreEqual(static_cast<ICanvasDevice*>(customDevice2.Get()), retrievedDevice.Get());
    }

    TEST_SHARED_CONTROL_BEHAVIOR(CustomDevice_CanBeCleared)
    {
        DeviceCreationFixture<TRAITS> f;

        auto customDevice = Make<StubCanvasDevice>();
        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(customDevice.Get()));

        f.EnsureDevice();

        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(nullptr));

        f.EnsureDevice();

        ComPtr<ICanvasDevice> retrievedDevice;
        Assert::AreEqual(S_OK, f.Control->get_CustomDevice(&retrievedDevice));

        Assert::IsNull(retrievedDevice.Get());
    }

    TEST_SHARED_CONTROL_BEHAVIOR(IfCustomDeviceIsUsed_NoDeviceIsCreatedOrRetrievedShared)
    {
        DeviceCreationFixture<TRAITS> f;

        auto customDevice = Make<StubCanvasDevice>();
        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(customDevice.Get()));

        f.DoNotExpectGetSharedDevice();
        f.DoNotExpectCreateDevice();

        f.EnsureDevice();
    }

    TEST_SHARED_CONTROL_BEHAVIOR(CustomDevice_RedundantSetToNull_EarliesOut)
    {
        DeviceCreationFixture<TRAITS> f;

        f.DoNotExpectChanged();

        for (int i = 0; i < 3; ++i)
        {
            Assert::AreEqual(S_OK, f.Control->put_CustomDevice(nullptr));
        }
    }

    TEST_SHARED_CONTROL_BEHAVIOR(CustomDevice_RedundantSetToSameDevice_EarliesOut)
    {
        DeviceCreationFixture<TRAITS> f;

        auto customDevice = Make<StubCanvasDevice>();
        Assert::AreEqual(S_OK, f.Control->put_CustomDevice(customDevice.Get()));

        f.DoNotExpectChanged();

        for (int i = 0; i < 3; ++i)
        {
            Assert::AreEqual(S_OK, f.Control->put_CustomDevice(customDevice.Get()));
        }
    }
};