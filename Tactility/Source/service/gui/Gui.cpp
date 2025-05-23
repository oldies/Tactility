#include "Tactility/service/gui/Gui.h"
#include "Tactility/lvgl/LvglSync.h"
#include "Tactility/lvgl/Statusbar.h"
#include "Tactility/lvgl/Style.h"
#include "Tactility/service/loader/Loader_i.h"

#include <Tactility/Tactility.h>
#include <Tactility/RtosCompat.h>

namespace tt::service::gui {

#define TAG "gui"

// Forward declarations
void redraw(Gui*);
static int32_t guiMain(TT_UNUSED void* p);

Gui* gui = nullptr;

void onLoaderMessage(const void* message, TT_UNUSED void* context) {
    auto* event = static_cast<const loader::LoaderEvent*>(message);
    if (event->type == loader::LoaderEventTypeApplicationShowing) {
        auto app_instance = app::getCurrentAppContext();
        showApp(app_instance);
    } else if (event->type == loader::LoaderEventTypeApplicationHiding) {
        hideApp();
    }
}

Gui* gui_alloc() {
    auto* instance = new Gui();
    tt_check(instance != nullptr);
    instance->thread = new Thread(
        "gui",
        4096, // Last known minimum was 2800 for launching desktop
        &guiMain,
        nullptr
    );
    instance->loader_pubsub_subscription = loader::getPubsub()->subscribe(&onLoaderMessage, instance);
    tt_check(lvgl::lock(1000 / portTICK_PERIOD_MS));
    instance->keyboardGroup = lv_group_create();
    auto* screen_root = lv_scr_act();
    assert(screen_root != nullptr);

    lvgl::obj_set_style_bg_blacken(screen_root);

    lv_obj_t* vertical_container = lv_obj_create(screen_root);
    lv_obj_set_size(vertical_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(vertical_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(vertical_container, 0, 0);
    lv_obj_set_style_pad_gap(vertical_container, 0, 0);
    lvgl::obj_set_style_bg_blacken(vertical_container);

    instance->statusbarWidget = lvgl::statusbar_create(vertical_container);

    auto* app_container = lv_obj_create(vertical_container);
    lv_obj_set_style_pad_all(app_container, 0, 0);
    lv_obj_set_style_border_width(app_container, 0, 0);
    lvgl::obj_set_style_bg_blacken(app_container);
    lv_obj_set_width(app_container, LV_PCT(100));
    lv_obj_set_flex_grow(app_container, 1);
    lv_obj_set_flex_flow(app_container, LV_FLEX_FLOW_COLUMN);

    instance->appRootWidget = app_container;

    lvgl::unlock();

    return instance;
}

void gui_free(Gui* instance) {
    assert(instance != nullptr);
    delete instance->thread;

    lv_group_delete(instance->keyboardGroup);
    tt_check(lvgl::lock(1000 / portTICK_PERIOD_MS));
    lv_group_del(instance->keyboardGroup);
    lvgl::unlock();

    delete instance;
}

void lock() {
    assert(gui);
    tt_check(gui->mutex.lock(configTICK_RATE_HZ));
}

void unlock() {
    assert(gui);
    tt_check(gui->mutex.unlock());
}

void requestDraw() {
    assert(gui);
    ThreadId thread_id = gui->thread->getId();
    Thread::setFlags(thread_id, GUI_THREAD_FLAG_DRAW);
}

void showApp(std::shared_ptr<app::AppContext> app) {
    lock();
    tt_check(gui->appToRender == nullptr);
    gui->appToRender = std::move(app);
    unlock();
    requestDraw();
}

void hideApp() {
    lock();
    tt_check(gui->appToRender != nullptr);

    // We must lock the LVGL port, because the viewport hide callbacks
    // might call LVGL APIs (e.g. to remove the keyboard from the screen root)
    tt_check(lvgl::lock(configTICK_RATE_HZ));
    gui->appToRender->getApp()->onHide(*gui->appToRender);
    lvgl::unlock();

    gui->appToRender = nullptr;
    unlock();
}

static int32_t guiMain(TT_UNUSED void* p) {
    tt_check(gui);
    Gui* local_gui = gui;

    while (true) {
        uint32_t flags = Thread::awaitFlags(GUI_THREAD_FLAG_ALL, EventFlag::WaitAny, (uint32_t)portMAX_DELAY);

        // Process and dispatch draw call
        if (flags & GUI_THREAD_FLAG_DRAW) {
            Thread::clearFlags(GUI_THREAD_FLAG_DRAW);
            redraw(local_gui);
        }

        if (flags & GUI_THREAD_FLAG_EXIT) {
            Thread::clearFlags(GUI_THREAD_FLAG_EXIT);
            break;
        }
    }

    return 0;
}

// region AppManifest

class GuiService : public Service {

public:

    void onStart(TT_UNUSED ServiceContext& service) override {
        assert(gui == nullptr);
        gui = gui_alloc();

        gui->thread->setPriority(THREAD_PRIORITY_SERVICE);
        gui->thread->start();
    }

    void onStop(TT_UNUSED ServiceContext& service) override {
        assert(gui != nullptr);
        lock();

        ThreadId thread_id = gui->thread->getId();
        Thread::setFlags(thread_id, GUI_THREAD_FLAG_EXIT);
        gui->thread->join();
        delete gui->thread;

        unlock();

        gui_free(gui);
    }
};

extern const ServiceManifest manifest = {
    .id = "Gui",
    .createService = create<GuiService>
};

// endregion

} // namespace
