import SwiftUI
import AppKit

@main
struct SonicPatchDemoApp: App {
    @StateObject private var model: DemoModel

    init() {
        if ProcessInfo.processInfo.arguments.contains("--smoke-test") {
            print("SonicPatch demo: native executable loaded successfully")
            exit(0)
        }
        _model = StateObject(wrappedValue: DemoModel())
    }

    var body: some Scene {
        Window("SonicPatch", id: "mixer") {
            MixerView(compact: false)
                .environmentObject(model)
                .frame(minWidth: 480, idealWidth: 540, minHeight: 540, idealHeight: 700)
        }
        .defaultSize(width: 540, height: 700)
        .windowStyle(.hiddenTitleBar)
        .commands {
            CommandGroup(replacing: .newItem) {}
        }
        MenuBarExtra("SonicPatch", systemImage: model.enabled ? "waveform" : "slider.horizontal.3") {
            MixerView(compact: true)
                .environmentObject(model)
                .frame(width: 400, height: 580)
        }
        .menuBarExtraStyle(.window)
    }
}
