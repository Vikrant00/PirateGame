// ============================================================
// PirateGame — Jenkins CI Pipeline
//
// Requirements on the Jenkins Windows agent:
//   - UE 5.7 installed, UE_PATH env var set (e.g. D:\unreal\UE_5.7)
//   - Git + Git LFS installed
//   - Sufficient disk (~20 GB for cook)
//
// Set these in Jenkins > Manage Jenkins > System > Global Properties:
//   UE_PATH = D:\unreal\UE_5.7
// ============================================================

pipeline {
    agent { label 'windows-ue5' }  // tag your Jenkins Windows node with this label

    parameters {
        choice(
            name: 'BUILD_CONFIG',
            choices: ['Development', 'Shipping', 'Debug'],
            description: 'Build configuration'
        )
        booleanParam(
            name: 'COOK_GAME',
            defaultValue: false,
            description: 'Cook and package the game after building (slow ~30-60 min)'
        )
        booleanParam(
            name: 'CLEAN_BUILD',
            defaultValue: false,
            description: 'Delete Binaries and Intermediate before building (full recompile)'
        )
    }

    environment {
        PROJECT_FILE = "${WORKSPACE}\\PirateGame.uproject"
        BUILD_BAT    = "${UE_PATH}\\Engine\\Build\\BatchFiles\\Build.bat"
        UAT          = "${UE_PATH}\\Engine\\Build\\BatchFiles\\RunUAT.bat"
        UBT          = "${UE_PATH}\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.exe"
        OUTPUT_DIR   = "${WORKSPACE}\\Build\\WindowsNoEditor"
    }

    options {
        timestamps()
        timeout(time: 2, unit: 'HOURS')
        buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {

        stage('Checkout') {
            steps {
                checkout scm
                bat 'git lfs pull'
                echo "Branch: ${env.BRANCH_NAME} | Config: ${params.BUILD_CONFIG}"
            }
        }

        stage('Clean') {
            when { expression { params.CLEAN_BUILD } }
            steps {
                echo 'Cleaning Binaries and Intermediate...'
                bat '''
                    if exist Binaries   rmdir /s /q Binaries
                    if exist Intermediate rmdir /s /q Intermediate
                '''
            }
        }

        stage('Generate Project Files') {
            steps {
                bat """
                    "%UBT%" -projectfiles -project="%PROJECT_FILE%" -game -rocket -progress
                """
            }
        }

        stage('Build Editor') {
            steps {
                bat """
                    call "%BUILD_BAT%" PirateGameEditor Win64 %BUILD_CONFIG%Editor ^
                        -Project="%PROJECT_FILE%" ^
                        -WaitMutex
                """
            }
            post {
                failure {
                    echo 'BUILD FAILED — check compile errors above.'
                }
            }
        }

        stage('Cook & Package') {
            when { expression { params.COOK_GAME } }
            steps {
                echo "Cooking for Windows (${params.BUILD_CONFIG})..."
                bat """
                    call "%UAT%" BuildCookRun ^
                        -project="%PROJECT_FILE%" ^
                        -noP4 ^
                        -platform=Win64 ^
                        -clientconfig=%BUILD_CONFIG% ^
                        -cook ^
                        -allmaps ^
                        -build ^
                        -stage ^
                        -pak ^
                        -archive ^
                        -archivedirectory="%OUTPUT_DIR%"
                """
            }
        }

        stage('Archive Build') {
            when { expression { params.COOK_GAME } }
            steps {
                archiveArtifacts(
                    artifacts: 'Build/WindowsNoEditor/**/*',
                    fingerprint: true,
                    allowEmptyArchive: false
                )
                echo "Build archived. Find it in Jenkins artifacts."
            }
        }
    }

    post {
        success {
            echo "Pipeline SUCCESS — ${env.BRANCH_NAME} @ ${env.BUILD_NUMBER}"
        }
        failure {
            echo "Pipeline FAILED — ${env.BRANCH_NAME} @ ${env.BUILD_NUMBER}"
            // Uncomment and configure to get email/Slack notifications:
            // mail to: 'team@yourproject.com', subject: "BUILD FAILED: ${env.JOB_NAME} #${env.BUILD_NUMBER}"
        }
        always {
            cleanWs(
                cleanWhenSuccess: false,   // keep workspace on success for faster next build
                cleanWhenFailure: false,
                cleanWhenAborted: true
            )
        }
    }
}
