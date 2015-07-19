import QtQuick 2.2
import QtQuick.Layouts 1.0

Item {
	id: root
	SystemPalette { id: palette }
	Layout.fillWidth: true
	
	Column {
		Rectangle {
			color: palette.dark
			height: 1
			width: root.width
		}
		Rectangle {
			color: palette.light
			height: 1
			width: root.width
		}
	}
}

