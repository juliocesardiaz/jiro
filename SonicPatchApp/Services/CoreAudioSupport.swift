//
//  CoreAudioSupport.swift
//  SonicPatch
//
//  THE single place for the AudioObjectGetPropertyData choreography. Every
//  Core Audio property read in the app — fixed-size PODs, CFString bridging,
//  and variable-length arrays — goes through these helpers instead of
//  hand-rolling the address/size/status dance at each call site.
//
//  Also hosts small domain conveniences (process-object list, bundle id,
//  default output device, device UID) shared by AppMonitor and ProcessTap.
//

#if canImport(CoreAudio)
import CoreAudio
import AudioToolbox

enum CoreAudioProperties {

    // MARK: Generic property plumbing

    /// Build a property address for `selector`, defaulting to the global
    /// scope / main element used by every read in this app.
    static func address(_ selector: AudioObjectPropertySelector,
                        scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal,
                        element: AudioObjectPropertyElement = kAudioObjectPropertyElementMain
    ) -> AudioObjectPropertyAddress {
        AudioObjectPropertyAddress(mSelector: selector, mScope: scope, mElement: element)
    }

    /// Read a fixed-size POD property (UInt32, pid_t, AudioObjectID,
    /// AudioStreamBasicDescription, ...). `initial` seeds the storage the HAL
    /// writes into; returns nil if the read fails.
    static func read<T>(_ type: T.Type,
                        object: AudioObjectID,
                        selector: AudioObjectPropertySelector,
                        initial: T) -> T? {
        var addr = address(selector)
        var value = initial
        var size = UInt32(MemoryLayout<T>.size)
        guard AudioObjectGetPropertyData(object, &addr, 0, nil, &size, &value) == noErr
        else { return nil }
        return value
    }

    /// Read a CFString property, bridged to String. The HAL writes a +1
    /// CFStringRef into raw storage; this helper owns that dance in ONE place:
    /// initialize a local `"" as CFString`, hand its address to
    /// AudioObjectGetPropertyData via withUnsafeMutablePointer, check noErr,
    /// and bridge the result.
    static func readString(object: AudioObjectID,
                           selector: AudioObjectPropertySelector) -> String? {
        var addr = address(selector)
        var value = "" as CFString
        var size = UInt32(MemoryLayout<CFString>.size)
        let status = withUnsafeMutablePointer(to: &value) {
            AudioObjectGetPropertyData(object, &addr, 0, nil, &size, $0)
        }
        guard status == noErr else { return nil }
        return value as String
    }

    /// Read a variable-length array property: AudioObjectGetPropertyDataSize
    /// to learn the byte count, then AudioObjectGetPropertyData into a buffer
    /// seeded with `placeholder`. Returns [] on any failure.
    static func readArray<T>(_ type: T.Type,
                             object: AudioObjectID,
                             selector: AudioObjectPropertySelector,
                             placeholder: T) -> [T] {
        var addr = address(selector)
        var size: UInt32 = 0
        guard AudioObjectGetPropertyDataSize(object, &addr, 0, nil, &size) == noErr,
              size > 0 else { return [] }
        var list = [T](repeating: placeholder, count: Int(size) / MemoryLayout<T>.size)
        guard AudioObjectGetPropertyData(object, &addr, 0, nil, &size, &list) == noErr
        else { return [] }
        return list
    }

    // MARK: Domain conveniences

    /// All process objects known to the HAL
    /// (`kAudioHardwarePropertyProcessObjectList` on the system object).
    static func processObjectList() -> [AudioObjectID] {
        readArray(AudioObjectID.self,
                  object: AudioObjectID(kAudioObjectSystemObject),
                  selector: kAudioHardwarePropertyProcessObjectList,
                  placeholder: AudioObjectID(kAudioObjectUnknown))
    }

    /// Bundle id of a process object (`kAudioProcessPropertyBundleID`).
    static func processBundleID(_ object: AudioObjectID) -> String? {
        readString(object: object, selector: kAudioProcessPropertyBundleID)
    }

    /// Whether the process is currently producing audio output
    /// (`kAudioProcessPropertyIsRunningOutput`). False if the read fails.
    static func processIsRunningOutput(_ object: AudioObjectID) -> Bool {
        (read(UInt32.self,
              object: object,
              selector: kAudioProcessPropertyIsRunningOutput,
              initial: 0) ?? 0) != 0
    }

    /// PID of a process object (`kAudioProcessPropertyPID`).
    static func processPID(_ object: AudioObjectID) -> pid_t? {
        read(pid_t.self, object: object, selector: kAudioProcessPropertyPID, initial: -1)
    }

    /// The current default output device
    /// (`kAudioHardwarePropertyDefaultOutputDevice` on the system object).
    /// Returns nil when the read fails or reports kAudioObjectUnknown.
    static func defaultOutputDeviceID() -> AudioObjectID? {
        guard let device = read(AudioObjectID.self,
                                object: AudioObjectID(kAudioObjectSystemObject),
                                selector: kAudioHardwarePropertyDefaultOutputDevice,
                                initial: AudioObjectID(kAudioObjectUnknown)),
              device != AudioObjectID(kAudioObjectUnknown)
        else { return nil }
        return device
    }

    /// UID of an audio device (`kAudioDevicePropertyDeviceUID`).
    static func deviceUID(_ device: AudioObjectID) -> String? {
        readString(object: device, selector: kAudioDevicePropertyDeviceUID)
    }
}

#endif // canImport(CoreAudio) — file contributes nothing on non-Apple toolchains.
