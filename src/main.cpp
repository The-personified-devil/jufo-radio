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
void menu_btn_cb(lv_obj_t *obj, lv_event_t event);
void local_play_btn_cb(lv_obj_t *obj, lv_event_t event);
void ml_mgr_list_cb(lv_obj_t *obj, lv_event_t event);
void ml_mgr_play_cb(lv_obj_t *obj, lv_event_t event);
void ml_mgr_create_cb(lv_obj_t *obj, lv_event_t event);
void ml_mgr_edit_cb(lv_obj_t *obj, lv_event_t event);
void ml_mgr_remove_cb(lv_obj_t *obj, lv_event_t event);
void fs_left_btn_cb(lv_obj_t *obj, lv_event_t event);
void fs_right_btn_cb(lv_obj_t *obj, lv_event_t event);
void file_selector_finish(lv_obj_t *obj, lv_event_t event);
void back_btn_cb(lv_obj_t *obj, lv_event_t event);
void src_ddl_evHandler(lv_obj_t *obj, lv_event_t event);
void sel_obj_dest_list_cb(lv_obj_t *obj, lv_event_t event);
void onEndReached();

lv_indev_t *kb_indev;
lv_indev_t *mouse_indev;

class named_mList
{
public:
   VLC::MediaList mList;
   std::string    name = "Unbenannte Playlist";
   long           key;

   named_mList(VLC::MediaList arg_mList, std::string arg_name, long arg_key)
   {
      mList = arg_mList;
      name  = arg_name;
      key   = arg_key;
   }

   named_mList() = default;
};

class mp
{
public:
   VLC::Instance                vlcInstance = VLC::Instance(0, nullptr);
   VLC::MediaPlayer             plr         = VLC::MediaPlayer(vlcInstance);
   VLC::MediaPlayerEventManager evMgr       = plr.eventManager();

   VLC::MediaList       current_mList = VLC::MediaList(vlcInstance);
   VLC::MediaListPlayer mListPlayer   = VLC::MediaListPlayer(vlcInstance);

   std::map < long, named_mList > mList_list;
   named_mList temp_mList;

   VLC::MediaList emptyList = VLC::MediaList(vlcInstance);

   mp()
   {
      auto regevent = evMgr.onEndReached(onEndReached);

      mListPlayer.setMediaPlayer(plr);
      mListPlayer.setMediaList(current_mList);
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
   virtual std::string get_type()
   {
      return("gui_level_base");
   }

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

   volatile bool    change_level_request = true;
   create_lvl_ptr_t create_lvl_req_ptr   = nullptr;

   std::vector < lv_obj_t * > to_delte_objs;
   volatile bool delete_obj_request = true;

   void change_gui_level(create_lvl_ptr_t create_lvl_ptr, create_lvl_ptr_t create_parent_lvl_ptr)
   {
      if (create_parent_lvl_ptr)
      {
         create_parent_func = create_parent_lvl_ptr;
      }

      if (create_lvl_ptr)
      {
         create_lvl_req_ptr   = create_lvl_ptr;
         change_level_request = true;
      }
   }

   void _change_gui_level()
   {
      if (create_lvl_req_ptr != nullptr)
      {
         delete gui_level;
         gui_level            = create_lvl_req_ptr();
         change_level_request = false;
         create_lvl_req_ptr   = nullptr;
      }
   }

   void delete_obj(lv_obj_t *obj)
   {
      to_delte_objs.push_back(obj);
      delete_obj_request = true;
   }

   void _delete_obj()
   {
      for (auto p : to_delte_objs)
      {
         if (p != nullptr)
         {
            lv_obj_del(p);
         }
      }
      to_delte_objs.clear();
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

      lv_obj_set_event_cb(back_btn, back_btn_cb);
   }
};

class main_menu: public gui_level_base    // Remember to disable back button
{
public:
   lv_obj_t *icon_page = lv_page_create(lv_scr_act(), NULL);
   lv_obj_t *local_icn = lv_imgbtn_create(icon_page, NULL);
   // lv_obj_t *spotify_icn    = lv_imgbtn_create(icon_page, NULL);

   lv_style_t hidden_style;
   lv_style_t visible_style;
   lv_style_t img_style;

   std::map < lv_obj_t *, gui_level_base *(*)() > menu_map;

   main_menu();

   ~main_menu();

   static gui_level_base *create();
};

class local_player: public gui_level_base
{
public:
   lv_obj_t *play_btn     = lv_imgbtn_create(lv_scr_act(), NULL);
   lv_obj_t *progress_bar = lv_slider_create(lv_scr_act(), NULL);

   lv_style_t hidden_style;
   lv_style_t play_btn_style;

   void *play_btn_icn_ptr;
   void *pause_btn_icn_ptr;
   void *play_again_btn_icn_ptr;

   local_player();

   ~local_player();

   void set_play_btn_img(bool clicked);

   std::string get_type();

   static gui_level_base *create();
};

class md_list_mgr: public gui_level_base    // Maybe integrate playlist preview
{
public:
   lv_obj_t *playlist_list = lv_list_create(lv_scr_act(), NULL);
   lv_obj_t *play_btn      = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t *create_btn    = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t *edit_btn      = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t *remove_btn    = lv_btn_create(lv_scr_act(), NULL);

   lv_obj_t *play_label   = lv_label_create(play_btn, NULL);
   lv_obj_t *create_label = lv_label_create(create_btn, NULL);
   lv_obj_t *edit_label   = lv_label_create(edit_btn, NULL);
   lv_obj_t *remove_label = lv_label_create(remove_btn, NULL);

   std::map < lv_obj_t *, named_mList > list_map;

   lv_obj_t *last_focused_list_btn = nullptr;

   md_list_mgr();

   ~md_list_mgr();

   static gui_level_base *create();
};

class file_selector: public gui_level_base
{
public:
   std::map < lv_obj_t *, std::pair < std::filesystem::directory_entry, VLC::Media >> file_map;
   std::map < lv_obj_t *, std::pair < std::filesystem::directory_entry, VLC::Media >> file_map_dest;
   lv_obj_t *src_list = lv_list_create(lv_scr_act(), NULL);
   lv_obj_t *md_list  = lv_list_create(lv_scr_act(), NULL);
//   lv_obj_t *up_btn     = lv_btn_create(lv_scr_act(), NULL);
//   lv_obj_t *down_btn   = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t *left_btn   = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t *right_btn  = lv_btn_create(lv_scr_act(), NULL);
   lv_obj_t *finish_btn = lv_btn_create(lv_scr_act(), NULL);

//   lv_obj_t *up_btn_label     = lv_label_create(up_btn, NULL);
//   lv_obj_t *down_btn_label   = lv_label_create(down_btn, NULL);
   lv_obj_t *left_btn_label   = lv_label_create(left_btn, NULL);
   lv_obj_t *right_btn_label  = lv_label_create(right_btn, NULL);
   lv_obj_t *finish_btn_label = lv_label_create(finish_btn, NULL);

   lv_style_t btn_style;
   lv_style_t btn_label_style;

   lv_obj_t *last_selected_btn      = nullptr;
   lv_obj_t *last_selected_btn_dest = nullptr;

   file_selector();

   ~file_selector();

   static gui_level_base *create();
};

main_menu::main_menu()
{
   lv_style_init(&hidden_style);
   lv_style_set_opa_scale(&hidden_style, LV_PAGE_PART_BG, LV_OPA_TRANSP);

   lv_style_init(&visible_style);
   lv_style_set_opa_scale(&visible_style, LV_BTN_PART_MAIN, LV_OPA_COVER);

   lv_style_init(&img_style);
   lv_style_set_image_recolor(&img_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
   lv_style_set_image_recolor_opa(&img_style, LV_STATE_DEFAULT, LV_OPA_COVER);

   lv_obj_set_size(icon_page, 600, 400);
   lv_obj_align(icon_page, NULL, LV_ALIGN_CENTER, 0, 0);
   lv_page_set_scrl_layout(icon_page, LV_LAYOUT_CENTER);
   lv_obj_add_style(icon_page, LV_PAGE_PART_BG, &hidden_style);

   LV_IMG_DECLARE(hdd_local_icn);
   lv_imgbtn_set_src(local_icn, LV_STATE_DEFAULT, &hdd_local_icn);
   lv_obj_add_style(local_icn, LV_STATE_DEFAULT, &img_style);
   lv_obj_add_style(local_icn, LV_BTN_PART_MAIN, &visible_style);
   lv_obj_set_event_cb(local_icn, menu_btn_cb);
   menu_map[local_icn] = local_player::create;
   // lv_obj_add_style(spotify_icn, LV_BTN_PART_MAIN, &visible_style);
}

main_menu::~main_menu()
{
   lv_obj_del(icon_page);
}

gui_level_base *main_menu::create()
{
   return(new main_menu);
}

local_player::local_player()
{
   lv_obj_align(play_btn, NULL, LV_ALIGN_CENTER, 0, 0);
   lv_obj_align(progress_bar, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

   lv_style_init(&hidden_style);
   lv_style_set_opa_scale(&hidden_style, LV_BTN_PART_MAIN, LV_OPA_TRANSP);

   lv_style_init(&play_btn_style);
   // lv_style_set_image_recolor(&play_btn_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
   lv_style_set_image_recolor(&play_btn_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
   lv_style_set_image_recolor_opa(&play_btn_style, LV_STATE_DEFAULT, LV_OPA_COVER);

   LV_IMG_DECLARE(play_btn_icn);
   LV_IMG_DECLARE(pause_btn_icn);
   LV_IMG_DECLARE(play_again_btn_icn);

   play_btn_icn_ptr       = (void *)&play_btn_icn;
   pause_btn_icn_ptr      = (void *)&pause_btn_icn;
   play_again_btn_icn_ptr = (void *)&play_again_btn_icn;

   lv_obj_add_style(play_btn, LV_BTN_PART_MAIN, &play_btn_style);
   lv_obj_set_event_cb(play_btn, local_play_btn_cb);
   set_play_btn_img(false);
}

local_player::~local_player()
{
   lv_obj_del(play_btn);
   lv_obj_del(progress_bar);
}

void local_player::set_play_btn_img(bool clicked)    // Add actions
{
   auto mp_state = mp_obj.mListPlayer.state();

   switch (mp_state)
   {
   case libvlc_Paused:
      lv_imgbtn_set_src(play_btn, LV_STATE_DEFAULT, play_btn_icn_ptr);
      if (clicked)
      {
         mp_obj.mListPlayer.play();
      }
      break;

   case libvlc_Stopped:
      lv_imgbtn_set_src(play_btn, LV_STATE_DEFAULT, play_btn_icn_ptr);
      if (clicked)
      {
         gui_mgr_obj.change_gui_level(md_list_mgr::create, local_player::create);
      }
      break;

   case libvlc_NothingSpecial:
      lv_imgbtn_set_src(play_btn, LV_STATE_DEFAULT, play_btn_icn_ptr);
      if (clicked)
      {
         gui_mgr_obj.change_gui_level(md_list_mgr::create, local_player::create);
      }
      break;

   case libvlc_Playing:
      lv_imgbtn_set_src(play_btn, LV_STATE_DEFAULT, pause_btn_icn_ptr);
      if (clicked)
      {
         mp_obj.mListPlayer.pause();
      }
      break;

   case libvlc_Ended:
      lv_imgbtn_set_src(play_btn, LV_STATE_DEFAULT, play_again_btn_icn_ptr);
      if (clicked)
      {
         mp_obj.mListPlayer.play();    // Maybe needs to go back to index one before playing again
      }
      break;

   default:
      break;
   }
}

std::string local_player::get_type()
{
   return("local_player");
}

gui_level_base *local_player::create()
{
   return(new local_player);
}

md_list_mgr::md_list_mgr()
{
   lv_obj_set_size(playlist_list, 450, 410);
   lv_obj_align(playlist_list, NULL, LV_ALIGN_IN_LEFT_MID, 5, 30);


   lv_obj_set_size(play_btn, 250, 50);
   lv_obj_align(play_btn, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -25, -225);
   lv_label_set_text(play_label, "Playlist abspielen");
   lv_obj_set_event_cb(play_btn, ml_mgr_play_cb);

   lv_obj_set_size(create_btn, 250, 50);
   lv_obj_align(create_btn, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -25, -155);
   lv_label_set_text(create_label, "Playlist erstellen");
   lv_obj_set_event_cb(create_btn, ml_mgr_create_cb);

   lv_obj_set_size(edit_btn, 250, 50);
   lv_obj_align(edit_btn, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -25, -85);
   lv_label_set_text(edit_label, "Playlist bearbeiten");
   lv_obj_set_event_cb(edit_btn, ml_mgr_edit_cb);

   lv_obj_set_size(remove_btn, 250, 50);
   lv_obj_align(remove_btn, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -25, -25);
   lv_label_set_text(remove_label, "Playlist entfernen");
   lv_obj_set_event_cb(remove_btn, ml_mgr_remove_cb);

   static lv_style_t btn_label_style;
   lv_style_init(&btn_label_style);
   lv_style_set_text_font(&btn_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_24);
   lv_obj_add_style(play_btn, LV_LABEL_PART_MAIN, &btn_label_style);
   lv_obj_add_style(create_btn, LV_LABEL_PART_MAIN, &btn_label_style);
   lv_obj_add_style(edit_btn, LV_LABEL_PART_MAIN, &btn_label_style);
   lv_obj_add_style(remove_btn, LV_LABEL_PART_MAIN, &btn_label_style);

   static lv_style_t play_btn_style;
   lv_style_init(&play_btn_style);
   // lv_style_set_bg_color(&play_label_style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
   //
   //
   auto iter = mp_obj.mList_list.begin();
   while (iter != mp_obj.mList_list.end())
   {
      lv_obj_t *list_btn = lv_list_add_btn(playlist_list, LV_SYMBOL_LIST, iter->second.name.c_str());
      list_map[list_btn] = iter->second;
      lv_obj_set_event_cb(list_btn, ml_mgr_list_cb);
      iter++;
   }

   std::cout << "Reached md_list_mgr" << std::endl;
}

md_list_mgr::~md_list_mgr()
{
   lv_obj_del(playlist_list);
   lv_obj_del(play_btn);
   lv_obj_del(create_btn);
   lv_obj_del(edit_btn);
   lv_obj_del(remove_btn);
}

gui_level_base *md_list_mgr::create()
{
   return(new md_list_mgr);
}

file_selector::file_selector()
{
   lv_obj_set_size(src_list, 390, 410);
   lv_obj_align(src_list, NULL, LV_ALIGN_IN_LEFT_MID, 5, 30);

   lv_obj_set_size(md_list, 390, 410);
   lv_obj_align(md_list, NULL, LV_ALIGN_IN_RIGHT_MID, -5, 30);
   lv_obj_t *src_list_scrbl = lv_page_get_scrollable(src_list);
   lv_obj_t *md_list_scrbl  = lv_page_get_scrollable(md_list);
   auto      dir_iterator   = std::filesystem::directory_iterator("bin");
   lv_obj_t *list_btn       = nullptr;



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
      }
   }

   int mdList_length = mp_obj.temp_mList.mList.count();
   std::cout << mdList_length << std::endl;
   for (int i = 0; i < mdList_length; i++)
   {mp_obj.temp_mList.mList.lock();
      VLC::Media *md = mp_obj.temp_mList.mList.itemAtIndex(i).get();
      mp_obj.temp_mList.mList.unlock();
      std::string fixed_mrl = md->mrl().erase(0,7);
      std::filesystem::directory_entry file_entr = std::filesystem::directory_entry(fixed_mrl);
      lv_obj_t* list_bttn = lv_list_add_btn(md_list, LV_SYMBOL_AUDIO, file_entr.path().filename().c_str()); 
      file_map_dest[list_bttn].first = file_entr;
      file_map_dest[list_bttn].second = md->duplicate();
      // std::cout << md->mrl() << std::endl;
      

      for (auto& p : file_map)
      {
         std::cout << p.first << " : " << p.second.first << " : " << p.second.second << '\n';
      }
   }
   lv_obj_set_size(left_btn, 50, 50);
   lv_obj_align(left_btn, NULL, LV_ALIGN_IN_TOP_MID, -30, 0);
   lv_obj_set_event_cb(left_btn, fs_left_btn_cb);

   lv_obj_set_size(right_btn, 50, 50);
   lv_obj_align(right_btn, NULL, LV_ALIGN_IN_TOP_MID, 30, 0);
   lv_obj_set_event_cb(right_btn, fs_right_btn_cb);


//   lv_obj_set_size(up_btn, 50, 50);
//   lv_obj_align(up_btn, NULL, LV_ALIGN_IN_TOP_MID, 170, 0);


//   lv_obj_set_size(down_btn, 50, 50);
//   lv_obj_align(down_btn, NULL, LV_ALIGN_IN_TOP_MID, 230, 0);


   lv_obj_set_size(finish_btn, 50, 50);
   lv_obj_align(finish_btn, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);

   lv_style_init(&btn_label_style);
   lv_style_set_opa_scale(&btn_label_style, LV_STATE_DEFAULT, LV_OPA_COVER);
   lv_style_set_text_font(&btn_label_style, LV_STATE_DEFAULT, &lv_font_montserrat_30);
   lv_obj_add_style(left_btn_label, LV_LABEL_PART_MAIN, &btn_label_style);
   lv_label_set_text(left_btn_label, LV_SYMBOL_LEFT);


   lv_obj_add_style(right_btn_label, LV_LABEL_PART_MAIN, &btn_label_style);
   lv_label_set_text(right_btn_label, LV_SYMBOL_RIGHT);


//   lv_obj_add_style(up_btn_label, LV_LABEL_PART_MAIN, &btn_label_style);
//   lv_label_set_text(up_btn_label, LV_SYMBOL_UP);


//   lv_obj_add_style(down_btn_label, LV_LABEL_PART_MAIN, &btn_label_style);
//   lv_label_set_text(down_btn_label, LV_SYMBOL_DOWN);


   lv_obj_add_style(finish_btn_label, LV_LABEL_PART_MAIN, &btn_label_style);
   lv_label_set_text(finish_btn_label, LV_SYMBOL_OK);

   lv_style_init(&btn_style);
   lv_style_set_opa_scale(&btn_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
   lv_obj_add_style(left_btn, LV_BTN_PART_MAIN, &btn_style);
   lv_obj_add_style(right_btn, LV_BTN_PART_MAIN, &btn_style);
//   lv_obj_add_style(up_btn, LV_BTN_PART_MAIN, &btn_style);
//   lv_obj_add_style(down_btn, LV_BTN_PART_MAIN, &btn_style);
   lv_obj_add_style(finish_btn, LV_BTN_PART_MAIN, &btn_style);

   lv_obj_set_event_cb(finish_btn, file_selector_finish);
}

file_selector::~file_selector()
{
   lv_obj_del(src_list);
   lv_obj_del(md_list);
}

gui_level_base *file_selector::create()
{
   return(new file_selector);
}

void menu_btn_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto menu = dynamic_cast < main_menu * > (gui_mgr_obj.gui_level);
      gui_mgr_obj.change_gui_level(menu->menu_map.at(obj), main_menu::create);
   }
}

void local_play_btn_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto local_player_obj = dynamic_cast < local_player * > (gui_mgr_obj.gui_level);
      local_player_obj->set_play_btn_img(true);
   }
}

void ml_mgr_list_cb(lv_obj_t *obj, lv_event_t event)
{
   if (is_double_tap(obj, event))
   {
      auto ml_mgr_obj = dynamic_cast < md_list_mgr * > (gui_mgr_obj.gui_level);

      mp_obj.mListPlayer.stop();
      mp_obj.mListPlayer.setMediaList(mp_obj.emptyList); // Is this even valid or do we have to set an empty playlist?
      mp_obj.current_mList = ml_mgr_obj->list_map.at(obj).mList;
      mp_obj.mListPlayer.setMediaList(mp_obj.current_mList);
      mp_obj.mListPlayer.play();
      gui_mgr_obj.change_gui_level(local_player::create, nullptr);
   }
   if (event == LV_EVENT_PRESSED)
   {
      auto ml_mgr_obj = dynamic_cast < md_list_mgr * > (gui_mgr_obj.gui_level);
      ml_mgr_obj->last_focused_list_btn = obj;
   }
}

void ml_mgr_play_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto md_list_mgr_obj = dynamic_cast < md_list_mgr * > (gui_mgr_obj.gui_level);
      if (md_list_mgr_obj->last_focused_list_btn != nullptr)
      {
         mp_obj.mListPlayer.stop();
         mp_obj.mListPlayer.setMediaList(mp_obj.emptyList); // Is this even valid or do we have to set an empty playlist?
         mp_obj.current_mList = md_list_mgr_obj->list_map.at(md_list_mgr_obj->last_focused_list_btn).mList;
         mp_obj.mListPlayer.setMediaList(mp_obj.current_mList);
         mp_obj.mListPlayer.play();
         std::this_thread::sleep_for(std::chrono::milliseconds(500));
         gui_mgr_obj.change_gui_level(local_player::create, nullptr);
      }
   }
}

void ml_mgr_create_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      mp_obj.temp_mList = named_mList(VLC::MediaList(mp_obj.vlcInstance), "Unbenannte Playlist", mp_obj.mList_list.size() + 1);
      gui_mgr_obj.change_gui_level(file_selector::create, nullptr);
   }
}

void ml_mgr_edit_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto md_list_mgr_obj = dynamic_cast < md_list_mgr * > (gui_mgr_obj.gui_level);
      if (md_list_mgr_obj->last_focused_list_btn != nullptr)
      {
         mp_obj.temp_mList = md_list_mgr_obj->list_map.at(md_list_mgr_obj->last_focused_list_btn);
         gui_mgr_obj.change_gui_level(file_selector::create, nullptr);
      }
   }
}

void ml_mgr_remove_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto md_list_mgr_obj = dynamic_cast < md_list_mgr * > (gui_mgr_obj.gui_level);
      if (md_list_mgr_obj->last_focused_list_btn != nullptr)
      {
         mp_obj.mList_list.erase(mp_obj.mList_list.find(md_list_mgr_obj->list_map.at(obj).key));
         gui_mgr_obj.change_gui_level(md_list_mgr::create, nullptr);
      }
   }
}

void back_btn_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      gui_mgr_obj.change_gui_level(gui_mgr_obj.create_parent_func, nullptr);
   }
}

void file_selector_finish(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);

      lv_obj_t *btn = lv_list_get_next_btn(file_selector_obj->md_list, NULL);
      if (btn != NULL)
      {
//         mp_obj.temp_mList.mList.lock();
//         mp_obj.temp_mList.mList = mp_obj.emptyList;
//         mp_obj.temp_mList.mList.unlock();
         while (btn != NULL)
         {
            auto& map_pair   = file_selector_obj->file_map_dest.at(btn);
            auto& media_file = map_pair.second;
            if (media_file)
            {
               // mp_obj.temp_mList.mList.lock();
               mp_obj.temp_mList.mList.addMedia(media_file);
               // mp_obj.temp_mList.mList.unlock();
            }
            btn = lv_list_get_next_btn(file_selector_obj->md_list, btn);
         }

         mp_obj.mList_list[mp_obj.temp_mList.key] = mp_obj.temp_mList;    // Does this even work?
         mp_obj.mListPlayer.stop();
         mp_obj.mListPlayer.setMediaList(mp_obj.emptyList);
         mp_obj.current_mList = mp_obj.emptyList; // Right?
      }
      gui_mgr_obj.change_gui_level(md_list_mgr::create, nullptr);
   }
}

void src_ddl_evHandler(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
      file_selector_obj->last_selected_btn = obj;
   }
}

// void fs_up_btn_cb(lv_obj_t *obj, lv_event_t event)
// {
//   if (event == LV_EVENT_PRESSED)
//   {
//      auto file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
//      file_selector_obj->last_selected_btn = obj;
//   }
//}

// void fs_down_btn_cb(lv_obj_t *obj, lv_event_t event)
// {
//   if (event == LV_EVENT_PRESSED)
//   {
//      auto file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
//      lv_obj_t* list = file_selector_obj->md_list;
//      lv_obj_t* l_sel_btn_dest = file_selector_obj->last_selected_btn_dest;
//      _lv_ll_move_before(list->child_ll, l_sel_btn_dest, lv_list_get_next_btn(list, l_sel_btn_dest)->child_ll)
//   }
//}

void fs_left_btn_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
      gui_mgr_obj.delete_obj(file_selector_obj->last_selected_btn_dest);
   }
}

void fs_right_btn_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto      file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
      lv_obj_t *right_obj         = file_selector_obj->last_selected_btn;
      lv_obj_t *list      = file_selector_obj->md_list;
      auto      file_type = file_selector_obj->file_map.at(right_obj).first;
      if (file_type.is_directory())
      {
      }
      {
         lv_obj_t *list_btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, lv_list_get_btn_text(right_obj));
         lv_obj_set_event_cb(list_btn, sel_obj_dest_list_cb);
         file_selector_obj->file_map_dest[list_btn].first  = file_type;
         file_selector_obj->file_map_dest[list_btn].second = file_selector_obj->file_map.at(right_obj).second;
      }
   }
}

void sel_obj_dest_list_cb(lv_obj_t *obj, lv_event_t event)
{
   if (event == LV_EVENT_PRESSED)
   {
      auto file_selector_obj = dynamic_cast < file_selector * > (gui_mgr_obj.gui_level);
      file_selector_obj->last_selected_btn_dest = obj;
   }
}

void onEndReached()
{
   std::string type = gui_mgr_obj.gui_level->get_type();

   if (type == "local_player")
   {
      std::cout << "End Reached" << std::endl;
      auto local_player_obj = dynamic_cast < local_player * > (gui_mgr_obj.gui_level);
      local_player_obj->set_play_btn_img(false);
   }
}

int main(int argc, char **argv)
{
   /*Initialize the Littlev graphics library*/
   lv_init();

   /*Initialize the HAL (display, input devices, tick) for LVGL*/
   hal_init();
   back_level_btn back_level_btn_obj;
   gui_mgr_obj.change_gui_level(main_menu::create, md_list_mgr::create);
   // gui_mgr_obj.change_gui_level(&back_level_btn::create, nullptr);

   while (1)
   {
      /* Periodically call the lv_task handler.
       * It could be done in a timer interrupt or an OS task too.*/
      lv_task_handler();
      if (gui_mgr_obj.change_level_request)
      {
         gui_mgr_obj._change_gui_level();
      }
      if (gui_mgr_obj.delete_obj_request)
      {
         gui_mgr_obj._delete_obj();
      }
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
   lv_disp_drv_init(&disp_drv);             /*Basic initialization*/
   disp_drv.buffer   = &disp_buf1;
   disp_drv.flush_cb = monitor_flush;
   lv_disp_drv_register(&disp_drv);

   /* Add the mouse as input device
    * Use the 'mouse' driver which reads the PC's mouse*/
   mouse_init();
   lv_indev_drv_t indev_drv;
   lv_indev_drv_init(&indev_drv);             /*Basic initialization*/
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
      SDL_Delay(5);               /*Sleep for 5 millisecond*/
      lv_tick_inc(5);             /*Tell LittelvGL that 5 milliseconds were elapsed*/
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
