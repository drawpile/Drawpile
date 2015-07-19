import QtQuick 2.2

Rectangle {
	readonly property int itemHeight: 20
	readonly property int tickWidth: 20
	readonly property int padding: 3
	property alias cursorPos: cursor.x

	id: root
	color: "white"

	MouseArea {
		id: mousearea
		anchors.fill: parent
	}

	Rectangle {
		property int pos: 0

		id: cursor
		y: 0
		z: 2
		height: root.height

		width: 3
		color: "#3daee9"
	}

	function initIndex() {
		var index = playback.getIndexItems();

		var xScale = tickWidth+padding
		var yScale = itemHeight+padding
		var xOffset = 0;

		// Create context name items
		for(var i=1;i<index.names.length;++i) {
			var name = index.names[i];
			var item = contextNameItem.createObject(root, {
				x: padding,
				y: i * yScale,
				height: itemHeight,
				text: name
			});
			if(item.width > xOffset) { xOffset = item.width; }
		}

		xOffset += padding * 2;

		// Create index items
		for(var i=0;i<index.items.length;++i) {
			var item = index.items[i];
			indexItem.createObject(root, {
				idx: i,
				x: xOffset + item.start * xScale,
				y: padding + item.row * yScale,
				z: 1,
				length: item.end-item.start+1,
				color: item.color,
				image: item.icon ? "image://theme/" + (isDark(item.color) ? "dark/" : "light/") + item.icon : null
			});
		}

		root.width = index.items[index.items.length-1].end * xScale + padding;
		root.height = index.names.length * yScale + padding;

		// Create snapshot markers
		for(var i=0;i<index.snapshots.length;++i) {
			var s = index.snapshots[i];
			snapshotItem.createObject(root, {
				x: xOffset + s * xScale,
				height: root.height
			})
		}

		playback.onIndexPositionChanged.connect(function(pos) {
			cursor.x = xOffset + pos * xScale + tickWidth/2 - 2;
		});

		mousearea.doubleClicked.connect(function(mouse) {
			var pos = (mouse.x - xOffset) / xScale;
			playback.jumpTo(pos);
		});
	}

	function isDark(c) {
		var lum = c.r * 0.216 + c.g * 0.7152 + c.b * 0.0722;
		return lum <= 0.5;
	}

	Component {
		id: contextNameItem
		Text {
			textFormat: Text.PlainText
			verticalAlignment: Text.AlignVCenter
		}
	}

	Component {
		id: indexItem

		Item {
			id: root
			property alias color: rect.color
			property alias image: icon.source
			property int length
			property int idx
			property bool flagged: false

			opacity: flagged ? 0.3 : 1.0

			Rectangle {
				id: shadow
				x: 2
				y: 2
				width: rect.width
				height: rect.height
				color: "#c0c0c0"
			}
			Rectangle {

				id: rect
				width: length * tickWidth - 2
				height: itemHeight - 2

				MouseArea {
					cursorShape: Qt.PointingHandCursor
					anchors.fill: parent
					onClicked: {
						root.flagged = !root.flagged
						playback.setIndexItemSilenced(root.idx, root.flagged)
					}
				}
			}
			Image {
				id: icon
				width: itemHeight - 2
				height: itemHeight - 2
			}
		}
	}

	Component {
		id: snapshotItem
		Rectangle {
			id: rect
			width: 1
			color: "#c0c0c0"
		}
	}
}

