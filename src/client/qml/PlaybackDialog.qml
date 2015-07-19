import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtQuick.Layouts 1.0
import QtQuick.Extras 1.4

import Controls 1.0

Rectangle {
	id: root

	SystemPalette { id: palette }
	color: palette.window

	states: [
		State {
			name: "SMALL"
			when: (width < 280 || height < 100)
			PropertyChanges { target: tinyplayer; visible: true }
			PropertyChanges { target: bigplayer; visible: false }
		}
	]

	// Normal (big) player mode
	ColumnLayout {
		id: bigplayer

		anchors.fill: parent
		anchors.margins: 4

		// Playback progress (read-only)
		ProgressBar {
			id: playbackProgress
			Layout.fillWidth: true
			style: PlaybackProgressStyle { }
			value: playback.progress
			maximumValue: playback.maxProgress
		}

		// Playback control buttons
		RowLayout {
			VcrButton {
				id: prevSequenceButton
				iconName: "media-skip-backward"
				enabled: false
				onClicked: playback.prevSequence()
			}
			VcrButton {
				id: playbutton
				size: 42
				iconName: "media-playback-start"
				checkedIconName: "media-playback-pause"
				checkable: true
				onClicked: playback.setPlaying(checked)
				Connections {
					target: playback
					onPlayingChanged: playbutton.checked = playback.playing
				}
			}
			VcrButton {
				iconName: "media-seek-forward"
				onClicked: playback.nextCommand()
			}
			VcrButton {
				iconName: "media-skip-forward"
				onClicked: playback.nextSequence()
			}

			Item { Layout.fillWidth: true }

			ToggleButton {
				id: enablePausesBtn

				implicitWidth: 42
				implicitHeight: 42

				checked: true

				onClicked: playback.setPauses(enablePausesBtn.checked)

				// iconName or Source doesn't seem to work with this type
				Image {
					anchors.centerIn: parent
					source: "image://theme/media-playback-pause"
				}
			}

			Dial {
				id: speedadjust
				implicitHeight: 42
				implicitWidth: 42

				minimumValue: 0
				maximumValue: 2.0
				value: 1
				stepSize: 0.1
				tickmarksVisible: false

				onValueChanged: {
					if(value <= 1.0)
						playback.speedFactor = value
					else
						playback.speedFactor = 1 + (value-1) * 8;
				}

				MouseArea {
					anchors.fill: parent
					propagateComposedEvents: true
					acceptedButtons: Qt.NoButton
					onWheel: {
						speedadjust.value = speedadjust.value + (wheel.angleDelta.y / (120*8))
					}
				}
			}
			Column {
				Label { text: qsTr("Speed") }
				Label {
					text: "x" + playback.speedFactor.toFixed(2)
				}
			}
		}

		// Separate control area from index area
		HLine { }

		// Recording index view
		Loader {
			id: indexCtrlsLoader
			sourceComponent: makeIndexCtrls
			Layout.fillHeight: true
			Layout.fillWidth: true

			Connections {
				target: playback
				onIndexLoaded: {
					prevSequenceButton.enabled = true
					prevSequenceButtonSmall.enabled = true
					indexCtrlsLoader.sourceComponent = indexCtrls
					indexCtrlsLoader.item.indexView.initIndex()
					prevMarkerButton.enabled = true
					nextMarkerButton.enabled = true
					prevMarkerButtonSmall.enabled = true
					nextMarkerButtonSmall.enabled = true
					addmarker.enabled = true
				}
			}
		}

		RowLayout {
			Button {
				text: qsTr("Markers")
				menu: Menu {
					id: markermenu
					MenuItem {
						id: stoponmarkers
						text: qsTr("Stop on markers")
						checkable: true
						checked: true

						onTriggered: playback.stopOnMarkers = checked
					}
					MenuItem {
						id: addmarker
						text: qsTr("Add here")
						enabled: false

						onTriggered: dialog.addMarker()
					}
					MenuSeparator { }
					Instantiator {
						model: playback.markers
						MenuItem {
							text: modelData
							onTriggered: playback.jumpToMarker(index)
						}
						onObjectAdded: markermenu.insertItem(index, object)
						onObjectRemoved: markermenu.removeItem(object)
					}
				}
				Connections {
					target: playback
					onStopOnMarkersChanged: stoponmarkers.checked = playback.stopOnMarkers
				}
			}

			VcrButton {
				id: prevMarkerButton
				iconName: "media-skip-backward"
				enabled: false
				onClicked: playback.prevMarker()
			}
			VcrButton {
				id: nextMarkerButton
				iconName: "media-skip-forward"
				enabled: false
				onClicked: playback.nextMarker()
			}

			Item { Layout.fillWidth: true }
			Button {
				text: qsTr("Filter...")
				onClicked: dialog.filterRecording()
			}
		}

		// Separate index area from video export controls
		HLine { }

		// Video export controls
		Loader {
			id: videoExportLoader
			sourceComponent: videoExportStart
			Layout.fillWidth: true

			Connections {
				target: playback
				onExportStarted: videoExportLoader.sourceComponent = videoExportCtrls
				onExportEnded: videoExportLoader.sourceComponent = videoExportStart
			}

			Component.onCompleted: playback.loadIndex()
		}
	}

	// Small player mode
	ColumnLayout {
		id: tinyplayer
		anchors.fill: parent
		anchors.margins: 2
		visible: false

		ProgressBar {
			Layout.fillWidth: true
			style: PlaybackProgressStyle { }
			value: playback.progress
			maximumValue: playback.maxProgress
		}

		RowLayout {
			Item { Layout.fillWidth: true }

			VcrButton {
				id: prevMarkerButtonSmall
				iconName: "media-skip-backward"
				enabled: false
				onClicked: playback.prevMarker()
				size: 22
			}
			VcrButton {
				id: prevSequenceButtonSmall
				iconName: "media-skip-backward"
				enabled: false
				onClicked: playback.prevSequence()
			}
			VcrButton {
				id: playbutton2
				iconName: "media-playback-start"
				checkedIconName: "media-playback-pause"
				checkable: true
				onClicked: playback.setPlaying(checked)
				Connections {
					target: playback
					onPlayingChanged: playbutton2.checked = playback.playing
				}
			}
			VcrButton {
				iconName: "media-seek-forward"
				onClicked: playback.nextCommand()
			}
			VcrButton {
				iconName: "media-skip-forward"
				onClicked: playback.nextSequence()
			}
			VcrButton {
				id: nextMarkerButtonSmall
				iconName: "media-skip-forward"
				enabled: false
				onClicked: playback.nextMarker()
				size: 22
			}

			Item { Layout.fillWidth: true }
		}
	}

	// This component is shown when an index has not been generated.
	Component {
		id: makeIndexCtrls
		ColumnLayout {
			Label {
				id: indexLoaderLabel
				text: ""
				horizontalAlignment: Text.AlignHCenter
				verticalAlignment: Text.AlignVCenter
				Layout.fillHeight: true
				Layout.fillWidth: true

				Connections {
					target: playback
					onIndexLoadError: {
						indexLoaderLabel.text = message
						buildIndexButton.enabled = canRetry
					}
				}

			}
			ProgressBar {
				id: buildIndexProgress
				visible: false
				Layout.fillWidth: true
				value: playback.indexBuildProgress
			}
			Button {
				id: buildIndexButton
				text: qsTr("Build index")
				Layout.alignment: Qt.AlignHCenter
				onClicked: {
					playback.buildIndex()
					buildIndexButton.visible = false
					buildIndexProgress.visible = true
				}
			}
		}
	}

	// This is shown when an index is loaded
	Component {
		id: indexCtrls

		ScrollView {
			property alias indexView: indexView
			id: indexViewArea
			Layout.fillWidth: true
			Layout.fillHeight: true
			PlaybackIndexView {
				id: indexView
				onCursorPosChanged: {
					var c = indexView.cursorPos;
					var x = c - indexViewArea.width / 2;
					if(x<0) { x = 0; }
					else if(x>width-indexViewArea.width) { x = width-indexViewArea.width; }
					indexViewArea.flickableItem.contentX = x;
				}
			}
		}
	}

	// Buttons for starting video export
	Component {
		id: videoExportStart
		RowLayout {
			Item { Layout.fillWidth: true }

			Button {
				text: qsTr("Export video...")
				onClicked: dialog.configureVideoExport()
			}
		}
	}

	// Buttons for controlling video export
	Component {
		id: videoExportCtrls

		RowLayout {

			Label { text: qsTr("Frame:") }
			Label { text: playback.currentExportFrame }
			Label { text: qsTr("Time:") }
			Label { text: playback.currentExportTime }

			Item { Layout.fillWidth: true }

			CheckBox {
				id: autoSave
				text: qsTr("Autosave")
				checked: true

				onClicked: playback.autosave = checked
				Connections {
					target: playback
					onAutosaveChanged: autoSave.checked = playback.autosave
				}
			}
			Button {
				text: qsTr("Save frame")
				onClicked: playback.exportFrame()
			}
			Button {
				text: qsTr("Stop")
				onClicked: playback.stopExporter()
			}
		}
	}
}

