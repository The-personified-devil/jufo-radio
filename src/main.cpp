#include <iostream>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <filesystem>
#include <map>
#include <thread>
#include <array>

#include <SDL2/SDL.h>
#include <vlcpp/vlc.hpp>

#include "../external/lvgl/lvgl/lvgl.h"
#include "../external/lvgl/lv_drivers/display/monitor.h"
#include "../external/lvgl/lv_drivers/indev/mouse.h"

#include "stuff.hpp"


//#define _DEFAULT_SOURCE /* needed for usleep() */
#define SDL_MAIN_HANDLED    /*To fix SDL's "undefined reference to WinMain" issue*/

static void hal_init(void);
static int tick_thread(void *data);
bool is_double_tap(lv_obj_t *& obj, lv_event_t&);

/* Callbacks */
void back_btn_cb(lv_obj_t *obj, lv_event_t event);
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

   VLC::MediaList       mList       = VLC::MediaList(vlcInstance);
   VLC::MediaListPlayer mListPlayer = VLC::MediaListPlayer(vlcInstance);

   mp()
   {
      auto regevent = evMgr.onEndReached(onEndReached);

      mListPlayer.setMediaPlayer(plr);
      mListPlayer.setMediaList(mList);
      // vlcInstance.addIntf((std::string)"");
   }

   void setMedia(std::string mediaFile)
   {
      auto media = VLC::Media(vlcInstance, mediaFile, VLC::Media::FromPath);
   }
} mp_obj;

class gui_level_base
{
public:
   virtual ~gui_level_base()
   {
   }
};

class gui_mgr
{
public:
   gui_level_base * gui_level = nullptr;
   gui_level_base * (*create_parent_func)() = nullptr;
   using            create_lvl_ptr_t = gui_level_base * (*)();
   void change_gui_level(create_lvl_ptr_t create_lvl_ptr, create_lvl_ptr_t create_parent_lvl_ptr)
   {
      if (create_parent_lvl_ptr)
      {
         create_parent_func = create_parent_lvl_ptr;
      }

      if (create_lvl_ptr)
      {
         delete gui_level;
         gui_level = create_lvl_ptr();
      }
   }
} gui_mgr_obj;

class back_level_btn
{
public:
   lv_obj_t * back_btn       = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t * back_btn_label = lv_label_create(back_btn, NULL);
   lv_style_t back_btn_style;
   lv_style_t back_btn_label_style;

   back_level_btn()
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
   }
};

class md_list_mgr: public gui_level_base
{
public:
   md_list_mgr ()
   {
      std::cout << "Reached md_list_mgr" << std::endl;
   }

   static gui_level_base* create()
   {
      return(new md_list_mgr);
   }
};

class file_selector: public gui_level_base
{
public:
   std::map < lv_obj_t *, std::pair < std::filesystem::directory_entry, VLC::Media >> file_map;
   lv_obj_t *src_list = lv_list_create(lv_scr_act(), NULL);
   lv_obj_t *md_list  = lv_list_create(lv_scr_act(), NULL);
   file_selector()
   {
      lv_obj_set_size(src_list, 390, 410);
      lv_obj_align(src_list, NULL, LV_ALIGN_IN_LEFT_MID, 5, 30);

      lv_obj_set_size(md_list, 390, 410);
      lv_obj_align(md_list, NULL, LV_ALIGN_IN_RIGHT_MID, -5, 30);
      lv_obj_t *src_list_scrbl = lv_page_get_scrollable(src_list);
      lv_obj_t *md_list_scrbl  = lv_page_get_scrollable(md_list);

      auto      dir_iterator = std::filesystem::directory_iterator("bin");
      lv_obj_t *list_btn     = nullptr;

      for (auto& p : dir_iterator)
      {
         std::cout << p.path() << '\n';
         VLC::Media media;
         auto       media_ext = make_array(".mp3", ".mp4", ".mkv", ".webm");

         if (p.is_directory())
         {
            list_btn = lv_list_add_btn(src_list, LV_SYMBOL_DIRECTORY, p.path().filename().c_str());
         }
         else
         {
            for (auto ext : media_ext)
            {
               if (ext == p.path().extension())
               {
                  list_btn = lv_list_add_btn(src_list, LV_SYMBOL_FILE, p.path().filename().c_str());
                  media    = VLC::Media(mp_obj.vlcInstance, p.path(), VLC::Media::FromPath);
               }
            }
         }
         if (list_btn)
         {
            lv_obj_set_event_cb(list_btn, src_ddl_evHandler);
            file_map[list_btn].first  = p;
            file_map[list_btn].second = media;
            lv_obj_set_parent(list_btn, md_list_scrbl);
         }
      }

      for (auto& p : file_map)
      {
         std::cout << p.first << " : " << p.second.first << " : " << p.second.second << '\n';
      }
   }
   
   ~file_selector()
   {
      lv_obj_del(src_list);
      lv_obj_del(md_list);
   }

   static gui_level_base *create()
   {
      return(new file_selector);
   }
};

void back_btn_cb(lv_obj_t *obj, lv_event_t event)
{
   gui_mgr_obj.change_gui_level(gui_mgr_obj.create_parent_func, nullptr);
}

void src_ddl_evHandler(lv_obj_t *obj, lv_event_t event)
{
   if (is_double_tap(obj, event))
   {
      lv_obj_t *list = lv_obj_get_parent(lv_obj_get_parent(obj));
      lv_obj_t *btn  = lv_list_get_next_btn(list, NULL);
      while (btn != NULL)
      {
         auto  file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
         auto& map_pair          = file_selector_obj->file_map.at(btn);
         auto& media_file        = map_pair.second;
         if (media_file)
         {
            mp_obj.mList.addMedia(media_file);
         }
         btn = lv_list_get_next_btn(list, btn);
      }
      mp_obj.mListPlayer.play();
      auto  file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
      file_selector_obj->file_map.clear();
      gui_mgr_obj.change_gui_level(md_list_mgr::create, nullptr);
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
   back_level_btn back_level_btn_obj;
   gui_mgr_obj.change_gui_level(file_selector::create, nullptr);
   // gui_mgr_obj.change_gui_level(&back_level_btn::create, nullptr);

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
