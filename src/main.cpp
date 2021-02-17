#include <iostream>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <filesystem>
#include <map>
#include <thread>

#include <SDL2/SDL.h>
#include <vlcpp/vlc.hpp>

#include "../external/lvgl/lvgl/lvgl.h"
#include "../external/lvgl/lv_drivers/display/monitor.h"
#include "../external/lvgl/lv_drivers/indev/mouse.h"


//#define _DEFAULT_SOURCE /* needed for usleep() */
#define SDL_MAIN_HANDLED    /*To fix SDL's "undefined reference to WinMain" issue*/

static void hal_init(void);
static int tick_thread(void *data);
static void memory_monitor(lv_task_t *param);
bool is_double_tap(lv_obj_t *& obj, lv_event_t&);

/* Callbacks */
void src_ddl_evHandler(lv_obj_t *obj, lv_event_t event);
void onEndReached();

lv_indev_t *kb_indev;
lv_indev_t *mouse_indev;

class mp
{
public:
   VLC::Instance                vlcInstance = VLC::Instance(0, nullptr);
   VLC::MediaPlayer             plr         = VLC::MediaPlayer(vlcInstance);
   VLC::MediaPlayerEventManager evMgr       = plr.eventManager();

   mp()
   {
      auto regevent = evMgr.onEndReached(onEndReached);
   }

   void setMedia(std::string mediaFile)
   {
      auto media = VLC::Media(vlcInstance, mediaFile, VLC::Media::FromPath);
   }
} mp_obj;

class gui_level_base
{
public:
   virtual gui_level_base *get_ptr()
   {
      return(this);
   }

   static gui_level_base *create()
   {
      return(new gui_level_base);
   };
};

class gui_mgr
{
public:
   gui_level_base * gui_level = nullptr;
   gui_level_base * (*create_parent_func)() = nullptr;

   using create_lvl_ptr_t = gui_level_base * (*)();
   void change_gui_level(create_lvl_ptr_t create_level_ptr)
   {
      gui_level = create_level_ptr();
      std::cout << gui_level->get_ptr() << std::endl;
   }
};

class back_btn: public gui_level_base
{
public:
   lv_obj_t * back_btn       = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t * back_btn_label = lv_label_create(back_btn, NULL);
   lv_style_t back_btn_style;
   lv_style_t back_btn_label_style;

   void init_back_btn()
   {
      lv_obj_set_size(back_btn, 50, 50);
      lv_obj_align(back_btn, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

      lv_style_init(&back_btn_label_style);
      lv_style_set_opa_scale(&back_btn_label_style, LV_STATE_DEFAULT, LV_OPA_COVER);
      lv_style_set_text_font(&back_btn_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_30);
      lv_obj_add_style(back_btn_label, LV_LABEL_PART_MAIN, &back_btn_label_style);
      lv_label_set_text(back_btn_label, LV_SYMBOL_LEFT);

      lv_style_init(&back_btn_style);
      lv_style_set_opa_scale(&back_btn_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
      lv_obj_add_style(back_btn, LV_BTN_PART_MAIN, &back_btn_style);
      // lv_obj_set_event_cb()
   }
};

class file_selector {
public:
   std::map < lv_obj_t *, std::pair < std::filesystem::directory_entry, VLC::Media >> file_map;

   void create_ddl()
   {
      lv_obj_t *src_list = lv_list_create(lv_scr_act(), NULL);

      lv_obj_set_size(src_list, 500, 200);
      lv_obj_align(src_list, NULL, LV_ALIGN_CENTER, 0, 0);


      auto      dir_iterator = std::filesystem::directory_iterator("bin");
      lv_obj_t *list_btn;

      for (auto& p : dir_iterator)
      {
         std::cout << p.path() << '\n';
         auto media = VLC::Media(mp_obj.vlcInstance, p.path().c_str(), VLC::Media::FromPath);

         if (p.is_directory())
         {
            list_btn = lv_list_add_btn(src_list, LV_SYMBOL_DIRECTORY, p.path().filename().c_str());
         }
         else
         {
            list_btn = lv_list_add_btn(src_list, LV_SYMBOL_FILE, p.path().filename().c_str());
         }

         lv_obj_set_event_cb(list_btn, src_ddl_evHandler);
         file_map[list_btn].first  = p;
         file_map[list_btn].second = media;
      }

      for (auto& p : file_map)
      {
         std::cout << p.first << " : " << p.second.first << " : " << p.second.second << '\n';
      }
   }
} file_selector_obj;


void src_ddl_evHandler(lv_obj_t *obj, lv_event_t event)
{
   if (is_double_tap(obj, event))
   {
      auto media_file = file_selector_obj.file_map.at(obj);
      mp_obj.plr.setMedia(media_file.second);
      mp_obj.plr.play();
   }
}

void onEndReached()
{
   std::cout << "Worked" << std::endl;
}

int main(int argc, char **argv)
{
   /*Initialize the Littlev graphics library*/
   lv_init();

   /*Initialize the HAL (display, input devices, tick) for LVGL*/
   hal_init();
   file_selector_obj.create_ddl();
   back_btn back_btn_obj;
   gui_mgr  gui_mgr_obj;
   back_btn_obj.init_back_btn();
   gui_mgr_obj.change_gui_level(&back_btn::create);

   while (1)
   {
      /* Periodically call the lv_task handler.
       * It could be done in a timer interrupt or an OS task too.*/
      lv_task_handler();
      usleep(5 * 1000);
   }
   return(0);
}

/**
 * Initialize the Hardware Abstraction Layer (HAL) for the Littlev graphics
 * library
 */
static void hal_init(void)
{
   /* Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/
   monitor_init();

   /*Create a display buffer*/
   static lv_disp_buf_t disp_buf1;
   static lv_color_t    buf1_1[LV_HOR_RES_MAX * 120];
   lv_disp_buf_init(&disp_buf1, buf1_1, NULL, LV_HOR_RES_MAX * 120);

   /*Create a display*/
   lv_disp_drv_t disp_drv;
   lv_disp_drv_init(&disp_drv); /*Basic initialization*/
   disp_drv.buffer   = &disp_buf1;
   disp_drv.flush_cb = monitor_flush;
   lv_disp_drv_register(&disp_drv);

   /* Add the mouse as input device
    * Use the 'mouse' driver which reads the PC's mouse*/
   mouse_init();
   lv_indev_drv_t indev_drv;
   lv_indev_drv_init(&indev_drv); /*Basic initialization*/
   indev_drv.type = LV_INDEV_TYPE_POINTER;

   /*This function will be called periodically (by the library) to get the mouse position and state*/
   indev_drv.read_cb = mouse_read;
   mouse_indev       = lv_indev_drv_register(&indev_drv);

   /* Tick init.
    * You have to call 'lv_tick_inc()' in periodically to inform LittelvGL about
    * how much time were elapsed Create an SDL thread to do this*/
   SDL_CreateThread(tick_thread, "tick", NULL);

   /* Optional:
    * Create a memory monitor task which prints the memory usage in
    * periodically.*/
   //lv_task_create(memory_monitor, 5000, LV_TASK_PRIO_MID, NULL);
}

/**
 * A task to measure the elapsed time for LVGL
 * @param data unused
 * @return never return
 */
static int tick_thread(void *data)
{
   (void)data;

   while (1)
   {
      SDL_Delay(5);   /*Sleep for 5 millisecond*/
      lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
   }

   return(0);
}

/**
 * Print the memory usage periodically
 * @param param
 */
static void memory_monitor(lv_task_t *param)
{
   (void)param; /*Unused*/

   lv_mem_monitor_t mon;
   lv_mem_monitor(&mon);
   printf("used: %6d (%3d %%), frag: %3d %%, biggest free: %6d\n",
          (int)mon.total_size - mon.free_size, mon.used_pct, mon.frag_pct,
          (int)mon.free_biggest_size);
}

/* Helper function to provid double click functionality. */
bool is_double_tap(lv_obj_t *& obj, lv_event_t& event)
{
   bool double_tap = false;

   if (event == LV_EVENT_SHORT_CLICKED)
   {
      static           std::chrono::time_point < std::chrono::steady_clock > time_point;
      static lv_obj_t *last_obj = obj;

      long long duration = std::chrono::duration_cast < std::chrono::milliseconds >
                           (std::chrono::steady_clock::now().time_since_epoch() - time_point.time_since_epoch()).count();
      time_point = std::chrono::steady_clock::now();

      if (duration <= 500 && last_obj == obj)
      {
         double_tap = true;
      }
      last_obj = obj;
   }
   return(double_tap);
}
