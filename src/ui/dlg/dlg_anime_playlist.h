/*
** Taiga
** Copyright (C) 2010-2017, Eren Okka
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>
#include <vector>

#include <windows/win/common_controls.h>
#include <windows/win/dialog.h>
#include <windows/win/gdi.h>
namespace anime {
	class Item;
}

namespace ui {

	class AnimePlaylistDialog : public win::Dialog {
	public:
		AnimePlaylistDialog() {};
		~AnimePlaylistDialog() {};
		INT_PTR DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		void OnContextMenu(HWND hwnd, POINT pt);
		BOOL OnInitDialog();
		LRESULT OnNotify(int idCtrl, LPNMHDR pnmh);
		void OnSize(UINT uMsg, UINT nType, SIZE size);

		int iPos;
		int                 iHeight;
		HIMAGELIST   hDragImageList;
		HIMAGELIST          hOneImageList, hTempImageList;
		LPNMHDR             pnmhdr;
		BOOL         bDragging;
		LVHITTESTINFO       lvhti;
		BOOL                bFirst;
		IMAGEINFO           imf;
		int GetCurrentId();
		void RebuildIdCache();
		int GetListIndex(int anime_id);

  std::vector<int> GetCurrentIds();
  anime::Item* GetCurrentItem();
  int GetCurrentStatus();
  bool LoadSavedUserPlaylist();
  void exportplaylisttofile(const std::wstring&  directoryname);
  void SchedulePlayListDate();
		void PushBack(int anime_id);

		void AddAnimeToList(int anime_id);
		void ParseResults(const std::vector<int>& ids);
		
		void RefreshList();

		void ParsePlaylistFile(const std::wstring&  playlistfile);
		
		const std::wstring PlayListFolderDestination();



		//std::vector<int> GetCurrentIds();
	public:
		// List-view control
		class ListView : public win::ListView {
		public:
			ListView();
			LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
			bool dragging;
			
		    win::Rect button_rect[3];
			bool button_visible[3];
			bool progress_bars_visible;
			win::ImageList drag_image;
			void DrawProgressBar(HDC hdc, RECT* rc, int index, anime::Item& anime_item);
			void DrawProgressText(HDC hdc, RECT* rc, anime::Item& anime_item);
			int hot_item;
			std::map<int, int> id_cache;
			win::Tooltip tooltips;
			AnimePlaylistDialog* parent;
			
		}listview;
	private:
		int current_status_;
		win::ListView list_;
		std::vector<int> anime_ids_;
		std::wstring search_text;

		int anime_id_;
		anime::Episode episode_;
		int episode_number_;
		std::wstring path_found_;
	};

	extern AnimePlaylistDialog DlgPlaylist;

}  // namespace ui
