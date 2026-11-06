//! A safe, thin Rust wrapper for ContactGlove C++/C/Rust SDK V2's stable C ABI.
//!
//! `Sdk` and `Subscription` intentionally do not implement `Send` or `Sync`.
//! Stream callbacks run on the SDK receiver thread; forward values through a
//! channel when another thread or an async runtime should consume them.

use std::array;
use std::ffi::{c_char, c_void};
use std::marker::PhantomData;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr::NonNull;
use std::rc::Rc;
use std::slice;

pub use contactglove_sdk_v2_sys as sys;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Error {
    SdkNotAvailable,
    NotConnected,
    InvalidId,
    InvalidArgument,
    BufferTooSmall,
    Unknown(u32),
}

pub type Result<T> = std::result::Result<T, Error>;

fn status(value: u32) -> Result<()> {
    match value {
        sys::CG_RESULT_SUCCESS => Ok(()),
        sys::CG_RESULT_SDK_NOT_AVAILABLE => Err(Error::SdkNotAvailable),
        sys::CG_RESULT_NOT_CONNECTED => Err(Error::NotConnected),
        sys::CG_RESULT_INVALID_ID => Err(Error::InvalidId),
        sys::CG_RESULT_INVALID_ARGUMENT => Err(Error::InvalidArgument),
        sys::CG_RESULT_BUFFER_TOO_SMALL => Err(Error::BufferTooSmall),
        other => Err(Error::Unknown(other)),
    }
}

fn string_view(value: &str) -> sys::cg_string_view_t {
    sys::cg_string_view_t {
        data: value.as_ptr().cast::<c_char>(),
        length: value.len(),
    }
}

unsafe fn owned_string(value: sys::cg_string_view_t) -> String {
    if value.data.is_null() || value.length == 0 {
        String::new()
    } else {
        String::from_utf8_lossy(slice::from_raw_parts(value.data.cast::<u8>(), value.length))
            .into_owned()
    }
}

struct OutputString {
    bytes: Vec<u8>,
    raw: sys::cg_string_buffer_t,
}

impl OutputString {
    fn new() -> Self {
        let mut output = Self {
            bytes: vec![0; 64],
            raw: sys::cg_string_buffer_t {
                data: std::ptr::null_mut(),
                capacity: 0,
                length: 0,
            },
        };
        output.refresh();
        output
    }

    fn refresh(&mut self) {
        self.raw.data = self.bytes.as_mut_ptr().cast::<c_char>();
        self.raw.capacity = self.bytes.len();
    }

    fn as_raw(&mut self) -> sys::cg_string_buffer_t {
        self.refresh();
        self.raw
    }

    fn capture(&mut self, raw: sys::cg_string_buffer_t) {
        self.raw.length = raw.length;
    }

    fn grow_if_needed(&mut self) -> bool {
        let required = self.raw.length.saturating_add(1);
        if required > self.bytes.len() {
            self.bytes.resize(required, 0);
            self.refresh();
            true
        } else {
            false
        }
    }

    fn value(&self) -> String {
        String::from_utf8_lossy(&self.bytes[..self.raw.length]).into_owned()
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Host {
    pub host_name: String,
    pub ip_address: String,
    pub port: u32,
    pub receive_port: u32,
    /// Local UDP port used for SDK-to-DivingStation messages. Zero lets the OS choose one.
    pub send_port: u32,
}

impl Default for Host {
    fn default() -> Self {
        Self {
            host_name: "localhost".into(),
            ip_address: "127.0.0.1".into(),
            port: 25791,
            receive_port: 25788,
            send_port: 0,
        }
    }
}

impl Host {
    fn as_raw(&self) -> sys::cg_host_t {
        sys::cg_host_t {
            host_name: string_view(&self.host_name),
            ip_address: string_view(&self.ip_address),
            port: self.port,
            receive_port: self.receive_port,
            send_port: self.send_port,
        }
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Quaternion {
    pub w: f32,
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Transform {
    pub position: Vec3,
    pub rotation: Quaternion,
}

impl From<sys::cg_transform_t> for Transform {
    fn from(value: sys::cg_transform_t) -> Self {
        Self {
            position: Vec3 {
                x: value.position.x,
                y: value.position.y,
                z: value.position.z,
            },
            rotation: Quaternion {
                w: value.rotation.w,
                x: value.rotation.x,
                y: value.rotation.y,
                z: value.rotation.z,
            },
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct GloveData {
    pub id: String,
    pub pair_id: String,
    pub battery_percentage: i32,
    pub is_haptics: bool,
    pub is_left: bool,
    pub family_type: u32,
    pub color: u32,
    pub connected_state: u32,
    pub name: String,
    pub is_external_power_present: bool,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ControllerData {
    pub id: String,
    pub is_left: bool,
    pub a_button: u32,
    pub b_button: u32,
    pub sys_button: u32,
    pub trigger_button: u32,
    pub joystick_button: u32,
    pub trackpad_button: u32,
    pub trigger: f32,
    pub grip_value: f32,
    pub grip_force: f32,
    pub joystick_x: f32,
    pub joystick_y: f32,
    pub trackpad_x: f32,
    pub trackpad_y: f32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ErgonomicsData {
    pub id: String,
    pub is_left: bool,
    pub generated_at: u64,
    pub tracking_state: u32,
    pub data: [f32; sys::CONTACTGLOVE_ERGONOMICS_DATA_COUNT as usize],
}

#[derive(Clone, Debug, PartialEq)]
pub struct SkeletonData {
    pub id: String,
    pub profile_uuid: String,
    pub is_left: bool,
    pub generated_at: u64,
    pub tracking_state: u32,
    pub transforms: [Transform; sys::CONTACTGLOVE_SKELETON_DATA_COUNT as usize],
}

#[derive(Clone, Debug, PartialEq)]
pub struct RawSkeletonData {
    pub id: String,
    pub is_left: bool,
    pub generated_at: u64,
    pub tracking_state: u32,
    pub transforms: [Transform; sys::CONTACTGLOVE_SKELETON_DATA_COUNT as usize],
}

#[derive(Clone, Debug, PartialEq)]
pub struct RetargetProfileInfo {
    pub request_id: i32,
    pub profile_uuid: String,
    pub status: i32,
    pub name: String,
    pub description: String,
}

#[derive(Clone, Debug, PartialEq)]
pub struct RawSensorTransformsData {
    pub id: String,
    pub is_left: bool,
    pub generated_at: u64,
    pub transforms: [Transform; sys::CONTACTGLOVE_RAW_SENSOR_TRANSFORM_COUNT as usize],
}

unsafe fn glove_from_view(value: &sys::cg_glove_data_view_t) -> GloveData {
    GloveData {
        id: owned_string(value.id),
        pair_id: owned_string(value.pair_id),
        battery_percentage: value.battery_percentage,
        is_haptics: value.is_haptics != 0,
        is_left: value.is_left != 0,
        family_type: value.family_type,
        color: value.color,
        connected_state: value.connected_state,
        name: owned_string(value.name),
        is_external_power_present: value.is_external_power_present != 0,
    }
}

unsafe fn controller_from_view(value: &sys::cg_controller_data_view_t) -> ControllerData {
    ControllerData {
        id: owned_string(value.id),
        is_left: value.is_left != 0,
        a_button: value.a_button,
        b_button: value.b_button,
        sys_button: value.sys_button,
        trigger_button: value.trigger_button,
        joystick_button: value.joystick_button,
        trackpad_button: value.trackpad_button,
        trigger: value.trigger,
        grip_value: value.grip_value,
        grip_force: value.grip_force,
        joystick_x: value.joystick_x,
        joystick_y: value.joystick_y,
        trackpad_x: value.trackpad_x,
        trackpad_y: value.trackpad_y,
    }
}

unsafe fn ergonomics_from_view(value: &sys::cg_ergonomics_data_view_t) -> ErgonomicsData {
    let values = slice::from_raw_parts(value.data, value.data_count);
    ErgonomicsData {
        id: owned_string(value.id),
        is_left: value.is_left != 0,
        generated_at: value.generated_at,
        tracking_state: value.tracking_state,
        data: array::from_fn(|index| values.get(index).copied().unwrap_or_default()),
    }
}

unsafe fn skeleton_from_view(value: &sys::cg_skeleton_data_view_t) -> SkeletonData {
    let values = slice::from_raw_parts(value.transforms, value.transform_count);
    SkeletonData {
        id: owned_string(value.id),
        profile_uuid: owned_string(value.profile_uuid),
        is_left: value.is_left != 0,
        generated_at: value.generated_at,
        tracking_state: value.tracking_state,
        transforms: array::from_fn(|index| {
            values
                .get(index)
                .copied()
                .map(Transform::from)
                .unwrap_or_default()
        }),
    }
}

unsafe fn raw_skeleton_from_view(value: &sys::cg_raw_skeleton_data_view_t) -> RawSkeletonData {
    let values = slice::from_raw_parts(value.transforms, value.transform_count);
    RawSkeletonData {
        id: owned_string(value.id),
        is_left: value.is_left != 0,
        generated_at: value.generated_at,
        tracking_state: value.tracking_state,
        transforms: array::from_fn(|index| {
            values
                .get(index)
                .copied()
                .map(Transform::from)
                .unwrap_or_default()
        }),
    }
}

unsafe fn profile_info_from_view(
    value: &sys::cg_retarget_profile_info_view_t,
) -> RetargetProfileInfo {
    RetargetProfileInfo {
        request_id: value.request_id,
        profile_uuid: owned_string(value.profile_uuid),
        status: value.status,
        name: owned_string(value.name),
        description: owned_string(value.description),
    }
}

unsafe fn raw_sensor_transforms_from_view(
    value: &sys::cg_raw_sensor_transforms_data_view_t,
) -> RawSensorTransformsData {
    let values = slice::from_raw_parts(value.transforms, value.transform_count);
    RawSensorTransformsData {
        id: owned_string(value.id),
        is_left: value.is_left != 0,
        generated_at: value.generated_at,
        transforms: array::from_fn(|index| {
            values
                .get(index)
                .copied()
                .map(Transform::from)
                .unwrap_or_default()
        }),
    }
}

struct CallbackContext<T> {
    callback: Box<dyn Fn(T) + Send + Sync + 'static>,
}

pub struct Subscription<'sdk> {
    sdk: NonNull<sys::cg_sdk_t>,
    id: sys::cg_subscription_t,
    context: *mut c_void,
    drop_context: unsafe fn(*mut c_void),
    _sdk: PhantomData<&'sdk Sdk>,
    _not_send_sync: PhantomData<Rc<()>>,
}

impl Drop for Subscription<'_> {
    fn drop(&mut self) {
        unsafe {
            let _ = sys::cg_sdk_unsubscribe(self.sdk.as_ptr(), self.id);
            (self.drop_context)(self.context);
        }
    }
}

unsafe fn drop_context<T>(context: *mut c_void) {
    drop(Box::from_raw(context.cast::<CallbackContext<T>>()));
}

pub struct Sdk {
    raw: NonNull<sys::cg_sdk_t>,
    _not_send_sync: PhantomData<Rc<()>>,
}

impl Sdk {
    pub fn new() -> Result<Self> {
        let mut raw = std::ptr::null_mut();
        unsafe {
            status(sys::cg_sdk_create(&mut raw))?;
        }
        let raw = NonNull::new(raw).ok_or(Error::SdkNotAvailable)?;
        Ok(Self {
            raw,
            _not_send_sync: PhantomData,
        })
    }

    pub fn abi_version() -> u32 {
        unsafe { sys::cg_get_abi_version() }
    }

    pub fn connect(&self, host: &Host) -> Result<()> {
        let host = host.as_raw();
        unsafe { status(sys::cg_sdk_connect(self.raw.as_ptr(), &host)) }
    }

    /// Do not call from a stream callback.
    pub fn disconnect(&self, host: &Host) -> Result<()> {
        let host = host.as_raw();
        unsafe { status(sys::cg_sdk_disconnect(self.raw.as_ptr(), &host)) }
    }

    pub fn does_glove_support_haptics(&self, id: &str) -> Result<bool> {
        let mut value = 0;
        unsafe {
            status(sys::cg_sdk_does_glove_support_haptics(
                self.raw.as_ptr(),
                string_view(id),
                &mut value,
            ))?;
        }
        Ok(value != 0)
    }

    pub fn glove_data(&self, id: &str) -> Result<GloveData> {
        let mut id_output = OutputString::new();
        let mut pair_output = OutputString::new();
        let mut name_output = OutputString::new();
        loop {
            let mut output: sys::cg_glove_data_t = unsafe { std::mem::zeroed() };
            output.id = id_output.as_raw();
            output.pair_id = pair_output.as_raw();
            output.name = name_output.as_raw();
            let result = unsafe {
                sys::cg_sdk_get_glove_data(self.raw.as_ptr(), string_view(id), &mut output)
            };
            id_output.capture(output.id);
            pair_output.capture(output.pair_id);
            name_output.capture(output.name);
            if result == sys::CG_RESULT_BUFFER_TOO_SMALL {
                id_output.grow_if_needed();
                pair_output.grow_if_needed();
                name_output.grow_if_needed();
                continue;
            }
            status(result)?;
            return Ok(GloveData {
                id: id_output.value(),
                pair_id: pair_output.value(),
                battery_percentage: output.battery_percentage,
                is_haptics: output.is_haptics != 0,
                is_left: output.is_left != 0,
                family_type: output.family_type,
                color: output.color,
                connected_state: output.connected_state,
                name: name_output.value(),
                is_external_power_present: output.is_external_power_present != 0,
            });
        }
    }

    pub fn controller_data(&self, id: &str) -> Result<ControllerData> {
        let mut id_output = OutputString::new();
        loop {
            let mut output: sys::cg_controller_data_t = unsafe { std::mem::zeroed() };
            output.id = id_output.as_raw();
            let result = unsafe {
                sys::cg_sdk_get_controller_data(self.raw.as_ptr(), string_view(id), &mut output)
            };
            id_output.capture(output.id);
            if result == sys::CG_RESULT_BUFFER_TOO_SMALL {
                id_output.grow_if_needed();
                continue;
            }
            status(result)?;
            return Ok(ControllerData {
                id: id_output.value(),
                is_left: output.is_left != 0,
                a_button: output.a_button,
                b_button: output.b_button,
                sys_button: output.sys_button,
                trigger_button: output.trigger_button,
                joystick_button: output.joystick_button,
                trackpad_button: output.trackpad_button,
                trigger: output.trigger,
                grip_value: output.grip_value,
                grip_force: output.grip_force,
                joystick_x: output.joystick_x,
                joystick_y: output.joystick_y,
                trackpad_x: output.trackpad_x,
                trackpad_y: output.trackpad_y,
            });
        }
    }

    pub fn ergonomics_data(&self, id: &str) -> Result<ErgonomicsData> {
        let mut id_output = OutputString::new();
        loop {
            let mut output: sys::cg_ergonomics_data_t = unsafe { std::mem::zeroed() };
            output.id = id_output.as_raw();
            let result = unsafe {
                sys::cg_sdk_get_ergonomics_data(self.raw.as_ptr(), string_view(id), &mut output)
            };
            id_output.capture(output.id);
            if result == sys::CG_RESULT_BUFFER_TOO_SMALL {
                id_output.grow_if_needed();
                continue;
            }
            status(result)?;
            return Ok(ErgonomicsData {
                id: id_output.value(),
                is_left: output.is_left != 0,
                generated_at: output.generated_at,
                tracking_state: output.tracking_state,
                data: output.data,
            });
        }
    }

    pub fn skeleton_data(&self, id: &str) -> Result<SkeletonData> {
        let mut id_output = OutputString::new();
        let mut profile_output = OutputString::new();
        loop {
            let mut output: sys::cg_skeleton_data_t = unsafe { std::mem::zeroed() };
            output.id = id_output.as_raw();
            output.profile_uuid = profile_output.as_raw();
            let result = unsafe {
                sys::cg_sdk_get_skeleton_data(self.raw.as_ptr(), string_view(id), &mut output)
            };
            id_output.capture(output.id);
            profile_output.capture(output.profile_uuid);
            if result == sys::CG_RESULT_BUFFER_TOO_SMALL {
                id_output.grow_if_needed();
                profile_output.grow_if_needed();
                continue;
            }
            status(result)?;
            return Ok(SkeletonData {
                id: id_output.value(),
                profile_uuid: profile_output.value(),
                is_left: output.is_left != 0,
                generated_at: output.generated_at,
                tracking_state: output.tracking_state,
                transforms: output.transforms.map(Transform::from),
            });
        }
    }

    pub fn raw_skeleton_data(&self, id: &str) -> Result<RawSkeletonData> {
        let mut id_output = OutputString::new();
        loop {
            let mut output: sys::cg_raw_skeleton_data_t = unsafe { std::mem::zeroed() };
            output.id = id_output.as_raw();
            let result = unsafe {
                sys::cg_sdk_get_raw_skeleton_data(self.raw.as_ptr(), string_view(id), &mut output)
            };
            id_output.capture(output.id);
            if result == sys::CG_RESULT_BUFFER_TOO_SMALL {
                id_output.grow_if_needed();
                continue;
            }
            status(result)?;
            return Ok(RawSkeletonData {
                id: id_output.value(),
                is_left: output.is_left != 0,
                generated_at: output.generated_at,
                tracking_state: output.tracking_state,
                transforms: output.transforms.map(Transform::from),
            });
        }
    }

    pub fn raw_sensor_transforms_data(&self, id: &str) -> Result<RawSensorTransformsData> {
        let mut id_output = OutputString::new();
        loop {
            let mut output: sys::cg_raw_sensor_transforms_data_t = unsafe { std::mem::zeroed() };
            output.id = id_output.as_raw();
            let result = unsafe {
                sys::cg_sdk_get_raw_sensor_transforms_data(
                    self.raw.as_ptr(),
                    string_view(id),
                    &mut output,
                )
            };
            id_output.capture(output.id);
            if result == sys::CG_RESULT_BUFFER_TOO_SMALL {
                id_output.grow_if_needed();
                continue;
            }
            status(result)?;
            return Ok(RawSensorTransformsData {
                id: id_output.value(),
                is_left: output.is_left != 0,
                generated_at: output.generated_at,
                transforms: output.transforms.map(Transform::from),
            });
        }
    }

    pub fn available_glove_ids(&self) -> Result<Vec<String>> {
        let mut count = 0;
        unsafe {
            status(sys::cg_sdk_get_available_glove_count(
                self.raw.as_ptr(),
                &mut count,
            ))?;
        }
        let mut ids = Vec::with_capacity(count as usize);
        for index in 0..count {
            let mut output = OutputString::new();
            loop {
                let mut raw = output.as_raw();
                let result = unsafe {
                    sys::cg_sdk_get_available_glove_id(self.raw.as_ptr(), index, &mut raw)
                };
                output.capture(raw);
                if result == sys::CG_RESULT_BUFFER_TOO_SMALL {
                    output.grow_if_needed();
                    continue;
                }
                status(result)?;
                ids.push(output.value());
                break;
            }
        }
        Ok(ids)
    }

    pub fn vibrate_glove(
        &self,
        id: &str,
        frequency: f32,
        amplitude: f32,
        duration: f32,
    ) -> Result<()> {
        unsafe {
            status(sys::cg_sdk_vibrate_glove(
                self.raw.as_ptr(),
                string_view(id),
                frequency,
                amplitude,
                duration,
            ))
        }
    }

    pub fn request_retarget_profile_info(&self, profile_uuid: &str, request_id: i32) -> Result<()> {
        unsafe {
            status(sys::cg_sdk_request_retarget_profile_info(
                self.raw.as_ptr(),
                string_view(profile_uuid),
                request_id,
            ))
        }
    }

    pub fn subscribe_glove_data<F>(&self, callback: F) -> Result<Subscription<'_>>
    where
        F: Fn(GloveData) + Send + Sync + 'static,
    {
        let context = Box::into_raw(Box::new(CallbackContext {
            callback: Box::new(callback),
        }));
        let mut id = 0;
        let result = unsafe {
            sys::cg_sdk_subscribe_glove_data(
                self.raw.as_ptr(),
                Some(glove_callback),
                context.cast(),
                &mut id,
            )
        };
        if let Err(error) = status(result) {
            unsafe {
                drop_context::<GloveData>(context.cast());
            }
            return Err(error);
        }
        Ok(self.subscription(id, context.cast(), drop_context::<GloveData>))
    }

    pub fn subscribe_controller_data<F>(&self, callback: F) -> Result<Subscription<'_>>
    where
        F: Fn(ControllerData) + Send + Sync + 'static,
    {
        let context = Box::into_raw(Box::new(CallbackContext {
            callback: Box::new(callback),
        }));
        let mut id = 0;
        let result = unsafe {
            sys::cg_sdk_subscribe_controller_data(
                self.raw.as_ptr(),
                Some(controller_callback),
                context.cast(),
                &mut id,
            )
        };
        if let Err(error) = status(result) {
            unsafe {
                drop_context::<ControllerData>(context.cast());
            }
            return Err(error);
        }
        Ok(self.subscription(id, context.cast(), drop_context::<ControllerData>))
    }

    pub fn subscribe_ergonomics_data<F>(&self, callback: F) -> Result<Subscription<'_>>
    where
        F: Fn(ErgonomicsData) + Send + Sync + 'static,
    {
        let context = Box::into_raw(Box::new(CallbackContext {
            callback: Box::new(callback),
        }));
        let mut id = 0;
        let result = unsafe {
            sys::cg_sdk_subscribe_ergonomics_data(
                self.raw.as_ptr(),
                Some(ergonomics_callback),
                context.cast(),
                &mut id,
            )
        };
        if let Err(error) = status(result) {
            unsafe {
                drop_context::<ErgonomicsData>(context.cast());
            }
            return Err(error);
        }
        Ok(self.subscription(id, context.cast(), drop_context::<ErgonomicsData>))
    }

    pub fn subscribe_skeleton_data<F>(&self, callback: F) -> Result<Subscription<'_>>
    where
        F: Fn(SkeletonData) + Send + Sync + 'static,
    {
        let context = Box::into_raw(Box::new(CallbackContext {
            callback: Box::new(callback),
        }));
        let mut id = 0;
        let result = unsafe {
            sys::cg_sdk_subscribe_skeleton_data(
                self.raw.as_ptr(),
                Some(skeleton_callback),
                context.cast(),
                &mut id,
            )
        };
        if let Err(error) = status(result) {
            unsafe {
                drop_context::<SkeletonData>(context.cast());
            }
            return Err(error);
        }
        Ok(self.subscription(id, context.cast(), drop_context::<SkeletonData>))
    }

    pub fn subscribe_raw_skeleton_data<F>(&self, callback: F) -> Result<Subscription<'_>>
    where
        F: Fn(RawSkeletonData) + Send + Sync + 'static,
    {
        let context = Box::into_raw(Box::new(CallbackContext {
            callback: Box::new(callback),
        }));
        let mut id = 0;
        let result = unsafe {
            sys::cg_sdk_subscribe_raw_skeleton_data(
                self.raw.as_ptr(),
                Some(raw_skeleton_callback),
                context.cast(),
                &mut id,
            )
        };
        if let Err(error) = status(result) {
            unsafe {
                drop_context::<RawSkeletonData>(context.cast());
            }
            return Err(error);
        }
        Ok(self.subscription(id, context.cast(), drop_context::<RawSkeletonData>))
    }

    pub fn subscribe_raw_sensor_transforms_data<F>(&self, callback: F) -> Result<Subscription<'_>>
    where
        F: Fn(RawSensorTransformsData) + Send + Sync + 'static,
    {
        let context = Box::into_raw(Box::new(CallbackContext {
            callback: Box::new(callback),
        }));
        let mut id = 0;
        let result = unsafe {
            sys::cg_sdk_subscribe_raw_sensor_transforms_data(
                self.raw.as_ptr(),
                Some(raw_sensor_transforms_callback),
                context.cast(),
                &mut id,
            )
        };
        if let Err(error) = status(result) {
            unsafe {
                drop_context::<RawSensorTransformsData>(context.cast());
            }
            return Err(error);
        }
        Ok(self.subscription(id, context.cast(), drop_context::<RawSensorTransformsData>))
    }

    pub fn subscribe_retarget_profile_info<F>(&self, callback: F) -> Result<Subscription<'_>>
    where
        F: Fn(RetargetProfileInfo) + Send + Sync + 'static,
    {
        let context = Box::into_raw(Box::new(CallbackContext {
            callback: Box::new(callback),
        }));
        let mut id = 0;
        let result = unsafe {
            sys::cg_sdk_subscribe_retarget_profile_info(
                self.raw.as_ptr(),
                Some(profile_info_callback),
                context.cast(),
                &mut id,
            )
        };
        if let Err(error) = status(result) {
            unsafe {
                drop_context::<RetargetProfileInfo>(context.cast());
            }
            return Err(error);
        }
        Ok(self.subscription(id, context.cast(), drop_context::<RetargetProfileInfo>))
    }

    fn subscription<'sdk>(
        &'sdk self,
        id: sys::cg_subscription_t,
        context: *mut c_void,
        drop_context: unsafe fn(*mut c_void),
    ) -> Subscription<'sdk> {
        Subscription {
            sdk: self.raw,
            id,
            context,
            drop_context,
            _sdk: PhantomData,
            _not_send_sync: PhantomData,
        }
    }
}

impl Drop for Sdk {
    fn drop(&mut self) {
        unsafe {
            sys::cg_sdk_destroy(self.raw.as_ptr());
        }
    }
}

unsafe extern "C" fn glove_callback(context: *mut c_void, value: *const sys::cg_glove_data_view_t) {
    if context.is_null() || value.is_null() {
        return;
    }
    let context = &*context.cast::<CallbackContext<GloveData>>();
    let value = glove_from_view(&*value);
    let _ = catch_unwind(AssertUnwindSafe(|| (context.callback)(value)));
}

unsafe extern "C" fn controller_callback(
    context: *mut c_void,
    value: *const sys::cg_controller_data_view_t,
) {
    if context.is_null() || value.is_null() {
        return;
    }
    let context = &*context.cast::<CallbackContext<ControllerData>>();
    let value = controller_from_view(&*value);
    let _ = catch_unwind(AssertUnwindSafe(|| (context.callback)(value)));
}

unsafe extern "C" fn ergonomics_callback(
    context: *mut c_void,
    value: *const sys::cg_ergonomics_data_view_t,
) {
    if context.is_null() || value.is_null() {
        return;
    }
    let context = &*context.cast::<CallbackContext<ErgonomicsData>>();
    let value = ergonomics_from_view(&*value);
    let _ = catch_unwind(AssertUnwindSafe(|| (context.callback)(value)));
}

unsafe extern "C" fn skeleton_callback(
    context: *mut c_void,
    value: *const sys::cg_skeleton_data_view_t,
) {
    if context.is_null() || value.is_null() {
        return;
    }
    let context = &*context.cast::<CallbackContext<SkeletonData>>();
    let value = skeleton_from_view(&*value);
    let _ = catch_unwind(AssertUnwindSafe(|| (context.callback)(value)));
}

unsafe extern "C" fn raw_skeleton_callback(
    context: *mut c_void,
    value: *const sys::cg_raw_skeleton_data_view_t,
) {
    if context.is_null() || value.is_null() {
        return;
    }
    let context = &*context.cast::<CallbackContext<RawSkeletonData>>();
    let value = raw_skeleton_from_view(&*value);
    let _ = catch_unwind(AssertUnwindSafe(|| (context.callback)(value)));
}

unsafe extern "C" fn raw_sensor_transforms_callback(
    context: *mut c_void,
    value: *const sys::cg_raw_sensor_transforms_data_view_t,
) {
    if context.is_null() || value.is_null() {
        return;
    }
    let context = &*context.cast::<CallbackContext<RawSensorTransformsData>>();
    let value = raw_sensor_transforms_from_view(&*value);
    let _ = catch_unwind(AssertUnwindSafe(|| (context.callback)(value)));
}

unsafe extern "C" fn profile_info_callback(
    context: *mut c_void,
    value: *const sys::cg_retarget_profile_info_view_t,
) {
    if context.is_null() || value.is_null() {
        return;
    }
    let context = &*context.cast::<CallbackContext<RetargetProfileInfo>>();
    let value = profile_info_from_view(&*value);
    let _ = catch_unwind(AssertUnwindSafe(|| (context.callback)(value)));
}
