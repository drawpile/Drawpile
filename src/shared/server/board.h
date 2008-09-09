#ifndef DP_SRV_BOARD_H
#define DP_SRV_BOARD_H

namespace server {

/**
 * Information about the drawing board.
 */
class Board {
	public:
		Board(int owner);

		/**
		 * Has the board been created yet?
		 */
		bool exists() const { return _exists; }

		/**
		 * Set board options
		 */
		void set(const QString& title, int w, int h);

		/**
		 * Set board options from a message
		 */
		bool set(const QStringList& tokens);

		/**
		 * Width of the board in pixels
		 */
		int width() const { return _width; }

		/**
		 * Height of the board in pixels
		 */
		int height() const { return _height; }

		/**
		 * The title of the board
		 */
		const QString& title() const { return _title; }

		/**
		 * Get a board info message representing this board.
		 */
		QString toMessage() const;

	private:
		bool _exists;
		QString _title;
		int _width;
		int _height;
		int _owner;
};

}

#endif

