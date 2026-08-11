import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 640
    height: 480
    visible: true
    title: qsTr("TESTI terve juhannuskasa")
    Button {
        id: addButton
        text: "Add..."
        font.pointSize: 24
        x: 20
        y:200
        onClicked: {
            // qmllint disable unqualified
            cppVideo.compress();
            // qmllint enable unqualified
        }
    }
}
